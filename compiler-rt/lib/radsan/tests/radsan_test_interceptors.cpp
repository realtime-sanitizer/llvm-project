/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "gtest/gtest.h"

#include <sanitizer_common/sanitizer_platform.h>
#include <sanitizer_common/sanitizer_platform_interceptors.h>

#include "radsan_test_utilities.h"

#if SANITIZER_APPLE
#include <libkern/OSAtomic.h>
#include <os/lock.h>
#endif

#include <atomic>
#include <chrono>
#include <thread>

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>

using namespace testing;
using namespace radsan_testing;
using namespace std::chrono_literals;

namespace {
void *fake_thread_entry_point(void *) { return nullptr; }
} // namespace

/*
    Allocation and deallocation
*/

TEST(TestRadsanInterceptors, mallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, malloc(1)); };
  expectRealtimeDeath(func, "malloc");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, reallocDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, realloc(ptr_1, 8)); };
  expectRealtimeDeath(func, "realloc");
  expectNonrealtimeSurvival(func);
}

#if SANITIZER_APPLE
TEST(TestRadsanInterceptors, reallocfDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, reallocf(ptr_1, 8)); };
  expectRealtimeDeath(func, "reallocf");
  expectNonrealtimeSurvival(func);
}
#endif

TEST(TestRadsanInterceptors, vallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, valloc(4)); };
  expectRealtimeDeath(func, "valloc");
  expectNonrealtimeSurvival(func);
}

#if SANITIZER_INTERCEPT_ALIGNED_ALLOC
TEST(TestRadsanInterceptors, alignedAllocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, aligned_alloc(16, 32)); };
  expectRealtimeDeath(func, "aligned_alloc");
  expectNonrealtimeSurvival(func);
}
#endif

// free_sized and free_aligned_sized (both C23) are not yet supported
TEST(TestRadsanInterceptors, freeDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto *ptr_2 = malloc(1);
  expectRealtimeDeath([ptr_1]() { free(ptr_1); }, "free");
  expectNonrealtimeSurvival([ptr_2]() { free(ptr_2); });

  // Prevent malloc/free pair being optimised out
  ASSERT_NE(nullptr, ptr_1);
  ASSERT_NE(nullptr, ptr_2);
}

TEST(TestRadsanInterceptors, freeSurvivesWhenRealtimeIfArgumentIsNull) {
  realtimeInvoke([]() { free(NULL); });
  expectNonrealtimeSurvival([]() { free(NULL); });
}

TEST(TestRadsanInterceptors, posixMemalignDiesWhenRealtime) {
  auto func = []() {
    void *mem;
    posix_memalign(&mem, 4, 4);
  };
  expectRealtimeDeath(func, "posix_memalign");
  expectNonrealtimeSurvival(func);
}

/*
    Sleeping
*/

TEST(TestRadsanInterceptors, sleepDiesWhenRealtime) {
  auto func = []() { sleep(0u); };
  expectRealtimeDeath(func, "sleep");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, usleepDiesWhenRealtime) {
  auto func = []() { usleep(1u); };
  expectRealtimeDeath(func, "usleep");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, nanosleepDiesWhenRealtime) {
  auto func = []() {
    auto t = timespec{};
    nanosleep(&t, &t);
  };
  expectRealtimeDeath(func, "nanosleep");
  expectNonrealtimeSurvival(func);
}

/*
    Filesystem
*/

TEST(TestRadsanInterceptors, openDiesWhenRealtime) {
  auto func = []() { open("./file.txt", O_RDONLY); };
  expectRealtimeDeath(func, "open");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, openatDiesWhenRealtime) {
  auto func = []() { openat(0, "./file.txt", O_RDONLY); };
  expectRealtimeDeath(func, "openat");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, creatDiesWhenRealtime) {
  auto func = []() { creat("./file.txt", O_TRUNC); };
  expectRealtimeDeath(func, "creat");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, fcntlDiesWhenRealtime) {
  auto func = []() { fcntl(0, F_GETFL); };
  expectRealtimeDeath(func, "fcntl");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, closeDiesWhenRealtime) {
  auto func = []() { close(0); };
  expectRealtimeDeath(func, "close");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, fopenDiesWhenRealtime) {
  auto func = []() {
    auto fd = fopen("./file.txt", "r");
    if (fd != nullptr)
      fclose(fd);
  };
  expectRealtimeDeath(func, "fopen");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, freadDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "r");

  // In some cases, like running under check-all, the file may not be created.
  if (fd != nullptr) {
    auto func = [fd]() {
      char c{};
      fread(&c, 1, 1, fd);
    };
    expectRealtimeDeath(func, "fread");
    expectNonrealtimeSurvival(func);
    fclose(fd);
  }
}

TEST(TestRadsanInterceptors, fwriteDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "w");

  // In some cases, like running under check-all, the file may not be created.
  if (fd != nullptr)
  {
    ASSERT_NE(nullptr, fd);
    auto message = "Hello, world!";
    auto func = [&]() { fwrite(&message, 1, 4, fd); };
    expectRealtimeDeath(func, "fwrite");
    expectNonrealtimeSurvival(func);
  }
}

TEST(TestRadsanInterceptors, fcloseDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "r");
  auto func = [fd]() { fclose(fd); };
  expectRealtimeDeath(func, "fclose");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, putsDiesWhenRealtime) {
  auto func = []() { puts("Hello, world!\n"); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, fputsDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "w");

  // In some cases, like running under check-all, the file may not be created.
  if (fd != nullptr) {
    auto func = [fd]() { fputs("Hello, world!\n", fd); };
    expectRealtimeDeath(func);
    expectNonrealtimeSurvival(func);
    fclose(fd);
  }
}

/*
    Concurrency
*/

TEST(TestRadsanInterceptors, pthreadCreateDiesWhenRealtime) {
  auto func = []() {
    auto thread = pthread_t{};
    auto const attr = pthread_attr_t{};
    struct thread_info *tinfo;
    pthread_create(&thread, &attr, &fake_thread_entry_point, tinfo);
  };
  expectRealtimeDeath(func, "pthread_create");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadMutexLockDiesWhenRealtime) {
  auto func = []() {
    auto mutex = pthread_mutex_t{};
    pthread_mutex_lock(&mutex);
  };

  expectRealtimeDeath(func, "pthread_mutex_lock");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadMutexUnlockDiesWhenRealtime) {
  auto func = []() {
    auto mutex = pthread_mutex_t{};
    pthread_mutex_unlock(&mutex);
  };

  expectRealtimeDeath(func, "pthread_mutex_unlock");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadMutexJoinDiesWhenRealtime) {
  auto func = []() {
    auto thread = pthread_t{};
    pthread_join(thread, nullptr);
  };

  expectRealtimeDeath(func, "pthread_join");
  expectNonrealtimeSurvival(func);
}

#if SANITIZER_APPLE

#pragma clang diagnostic push
// OSSpinLockLock is deprecated, but still in use in libc++
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST(TestRadsanInterceptors, osSpinLockLockDiesWhenRealtime) {
  auto func = []() {
    auto spin_lock = OSSpinLock{};
    OSSpinLockLock(&spin_lock);
  };
  expectRealtimeDeath(func, "OSSpinLockLock");
  expectNonrealtimeSurvival(func);
}
#pragma clang diagnostic pop

TEST(TestRadsanInterceptors, osUnfairLockLockDiesWhenRealtime) {
  auto func = []() {
    auto unfair_lock = os_unfair_lock_s{};
    os_unfair_lock_lock(&unfair_lock);
  };
  expectRealtimeDeath(func, "os_unfair_lock_lock");
  expectNonrealtimeSurvival(func);
}
#endif

#if SANITIZER_LINUX
TEST(TestRadsanInterceptors, spinLockLockDiesWhenRealtime) {
  auto spinlock = pthread_spinlock_t{};
  pthread_spin_init(&spinlock, PTHREAD_PROCESS_SHARED);
  auto func = [&]() { pthread_spin_lock(&spinlock); };
  expectRealtimeDeath(func, "pthread_spin_lock");
  expectNonrealtimeSurvival(func);
}
#endif

TEST(TestRadsanInterceptors, pthreadCondSignalDiesWhenRealtime) {
  auto func = []() {
    auto cond = pthread_cond_t{};
    pthread_cond_signal(&cond);
  };
  expectRealtimeDeath(func, "pthread_cond_signal");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadCondBroadcastDiesWhenRealtime) {
  auto func = []() {
    auto cond = pthread_cond_t{};
    pthread_cond_broadcast(&cond);
  };
  expectRealtimeDeath(func, "pthread_cond_broadcast");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadCondWaitDiesWhenRealtime) {
  auto cond = pthread_cond_t{};
  auto mutex = pthread_mutex_t{};
  ASSERT_EQ(0, pthread_cond_init(&cond, nullptr));
  ASSERT_EQ(0, pthread_mutex_init(&mutex, nullptr));
  auto func = [&]() { pthread_cond_wait(&cond, &mutex); };
  expectRealtimeDeath(func, "pthread_cond_wait");
  // It's very difficult to test the success case here without doing some
  // sleeping, which is at the mercy of the scheduler. What's really important
  // here is the interception - so we're only testing that for now.
}

TEST(TestRadsanInterceptors, pthreadRwlockRdlockDiesWhenRealtime) {
  auto func = []() {
    auto rwlock = pthread_rwlock_t{};
    pthread_rwlock_rdlock(&rwlock);
  };
  expectRealtimeDeath(func, "pthread_rwlock_rdlock");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadRwlockUnlockDiesWhenRealtime) {
  auto func = []() {
    auto rwlock = pthread_rwlock_t{};
    pthread_rwlock_unlock(&rwlock);
  };
  expectRealtimeDeath(func, "pthread_rwlock_unlock");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, pthreadRwlockWrlockDiesWhenRealtime) {
  auto func = []() {
    auto rwlock = pthread_rwlock_t{};
    pthread_rwlock_wrlock(&rwlock);
  };
  expectRealtimeDeath(func, "pthread_rwlock_wrlock");
  expectNonrealtimeSurvival(func);
}

/*
    Sockets
*/
TEST(TestRadsanInterceptors, openingASocketDiesWhenRealtime) {
  auto func = []() { socket(PF_INET, SOCK_STREAM, 0); };
  expectRealtimeDeath(func, "socket");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, sendToASocketDiesWhenRealtime) {
  auto func = []() { send(0, nullptr, 0, 0); };
  expectRealtimeDeath(func, "send");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, sendmsgToASocketDiesWhenRealtime) {
  auto const msg = msghdr{};
  auto func = [&]() { sendmsg(0, &msg, 0); };
  expectRealtimeDeath(func, "sendmsg");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, sendtoToASocketDiesWhenRealtime) {
  auto const addr = sockaddr{};
  auto const len = socklen_t{};
  auto func = [&]() { sendto(0, nullptr, 0, 0, &addr, len); };
  expectRealtimeDeath(func, "sendto");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, recvFromASocketDiesWhenRealtime) {
  auto func = []() { recv(0, nullptr, 0, 0); };
  expectRealtimeDeath(func, "recv");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, recvfromOnASocketDiesWhenRealtime) {
  auto addr = sockaddr{};
  auto len = socklen_t{};
  auto func = [&]() { recvfrom(0, nullptr, 0, 0, &addr, &len); };
  expectRealtimeDeath(func, "recvfrom");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, recvmsgOnASocketDiesWhenRealtime) {
  auto msg = msghdr{};
  auto func = [&]() { recvmsg(0, &msg, 0); };
  expectRealtimeDeath(func, "recvmsg");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, shutdownOnASocketDiesWhenRealtime) {
  auto func = [&]() { shutdown(0, 0); };
  expectRealtimeDeath(func, "shutdown");
  expectNonrealtimeSurvival(func);
}
