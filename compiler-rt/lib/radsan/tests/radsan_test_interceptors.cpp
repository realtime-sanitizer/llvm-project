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

#if SANITIZER_INTERCEPT_MEMALIGN || SANITIZER_INTERCEPT_PVALLOC
#include <malloc.h>
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

/*
  The creat function doesn't seem to work on an ubuntu Docker image when the
  path is in a shared volume of the host. For now, to keep testing convenient
  with a local Docker container, we just put it somewhere that's not in the
  shared volume (/tmp). This is volatile and will be cleaned up as soon as the
  container is stopped.
*/
constexpr const char *temporary_file_path() {
#if SANITIZER_LINUX
  return "/tmp/radsan_temporary_test_file.txt";
#elif SANITIZER_APPLE
  return "./radsan_temporary_test_file.txt";
#endif
}
} // namespace

/*
    Allocation and deallocation
*/

TEST(TestRadsanInterceptors, mallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, malloc(1)); };
  expectDeathInNonBlockingContext(func, "malloc");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, reallocDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, realloc(ptr_1, 8)); };
  expectDeathInNonBlockingContext(func, "realloc");
  expectSurvivalInBlockableContext(func);
}

#if SANITIZER_APPLE
TEST(TestRadsanInterceptors, reallocfDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, reallocf(ptr_1, 8)); };
  expectDeathInNonBlockingContext(func, "reallocf");
  expectSurvivalInBlockableContext(func);
}
#endif

TEST(TestRadsanInterceptors, vallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, valloc(4)); };
  expectDeathInNonBlockingContext(func, "valloc");
  expectSurvivalInBlockableContext(func);
}

#if SANITIZER_INTERCEPT_ALIGNED_ALLOC
TEST(TestRadsanInterceptors, alignedAllocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, aligned_alloc(16, 32)); };
  expectDeathInNonBlockingContext(func, "aligned_alloc");
  expectSurvivalInBlockableContext(func);
}
#endif

// free_sized and free_aligned_sized (both C23) are not yet supported
TEST(TestRadsanInterceptors, freeDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto *ptr_2 = malloc(1);
  expectDeathInNonBlockingContext([&ptr_1]() { free(ptr_1); }, "free");
  auto reset = [&]() { ptr_2 = malloc(1); };
  expectSurvivalInBlockableContext([&ptr_2]() { free(ptr_2); }, reset);

  // Prevent malloc/free pair being optimised out
  ASSERT_NE(nullptr, ptr_1);
  ASSERT_NE(nullptr, ptr_2);
}

TEST(TestRadsanInterceptors, freeSurvivesWhenRealtimeIfArgumentIsNull) {
  expectSurvivalInNonBlockingContext([]() { free(NULL); });
  expectSurvivalInBlockableContext([]() { free(NULL); });
}

TEST(TestRadsanInterceptors, posixMemalignDiesWhenRealtime) {
  auto func = []() {
    void *mem;
    posix_memalign(&mem, 4, 4);
  };
  expectDeathInNonBlockingContext(func, "posix_memalign");
  expectSurvivalInBlockableContext(func);
}

#if SANITIZER_INTERCEPT_MEMALIGN
TEST(TestRadsanInterceptors, memalignDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(memalign(2, 2048), nullptr); };
  expectDeathInNonBlockingContext(func, "memalign");
  expectSurvivalInBlockableContext(func);
}
#endif

#if SANITIZER_INTERCEPT_PVALLOC
TEST(TestRadsanInterceptors, pvallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(pvalloc(2048), nullptr); };
  expectDeathInNonBlockingContext(func, "pvalloc");
  expectSurvivalInBlockableContext(func);
}
#endif

/*
    Sleeping
*/

TEST(TestRadsanInterceptors, sleepDiesWhenRealtime) {
  auto func = []() { sleep(0u); };
  expectDeathInNonBlockingContext(func, "sleep");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, usleepDiesWhenRealtime) {
  auto func = []() { usleep(1u); };
  expectDeathInNonBlockingContext(func, "usleep");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, nanosleepDiesWhenRealtime) {
  auto func = []() {
    auto t = timespec{};
    nanosleep(&t, &t);
  };
  expectDeathInNonBlockingContext(func, "nanosleep");
  expectSurvivalInBlockableContext(func);
}

/*
    Filesystem
*/

TEST(TestRadsanInterceptors, openDiesWhenRealtime) {
  auto func = []() { open(temporary_file_path(), O_RDONLY); };
  expectDeathInNonBlockingContext(func, "open");
  expectSurvivalInBlockableContext(func);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, openatDiesWhenRealtime) {
  auto func = []() { openat(0, temporary_file_path(), O_RDONLY); };
  expectDeathInNonBlockingContext(func, "openat");
  expectSurvivalInBlockableContext(func);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, creatDiesWhenRealtime) {
  auto func = []() { creat(temporary_file_path(), S_IWOTH | S_IROTH); };
  expectDeathInNonBlockingContext(func, "creat");
  expectSurvivalInBlockableContext(func);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, fcntlDiesWhenRealtime) {
  auto func = []() { fcntl(0, F_GETFL); };
  expectDeathInNonBlockingContext(func, "fcntl");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, closeDiesWhenRealtime) {
  auto func = []() { close(0); };
  expectDeathInNonBlockingContext(func, "close");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, fopenDiesWhenRealtime) {
  auto func = []() {
    auto fd = fopen(temporary_file_path(), "w");
    EXPECT_THAT(fd, Ne(nullptr));
  };
  expectDeathInNonBlockingContext(func, "fopen");
  expectSurvivalInBlockableContext(func);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, freadDiesWhenRealtime) {
  auto fd = fopen(temporary_file_path(), "w");
  auto func = [fd]() {
    char c{};
    fread(&c, 1, 1, fd);
  };
  expectDeathInNonBlockingContext(func, "fread");
  expectSurvivalInBlockableContext(func);
  if (fd != nullptr)
    fclose(fd);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, fwriteDiesWhenRealtime) {
  auto fd = fopen(temporary_file_path(), "w");
  ASSERT_NE(nullptr, fd);
  auto message = "Hello, world!";
  auto func = [&]() { fwrite(&message, 1, 4, fd); };
  expectDeathInNonBlockingContext(func, "fwrite");
  expectSurvivalInBlockableContext(func);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, fcloseDiesWhenRealtime) {
  auto fd = fopen(temporary_file_path(), "w");
  EXPECT_THAT(fd, Ne(nullptr));
  auto func = [fd]() { fclose(fd); };
  expectDeathInNonBlockingContext(func, "fclose");
  expectSurvivalInBlockableContext(func);
  std::remove(temporary_file_path());
}

TEST(TestRadsanInterceptors, putsDiesWhenRealtime) {
  auto func = []() { puts("Hello, world!\n"); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, fputsDiesWhenRealtime) {
  auto fd = fopen(temporary_file_path(), "w");
  ASSERT_THAT(fd, Ne(nullptr)) << errno;
  auto func = [fd]() { fputs("Hello, world!\n", fd); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
  if (fd != nullptr)
    fclose(fd);
  std::remove(temporary_file_path());
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
  expectDeathInNonBlockingContext(func, "pthread_create");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadMutexLockDiesWhenRealtime) {
  auto func = []() {
    auto mutex = pthread_mutex_t{};
    pthread_mutex_lock(&mutex);
  };

  expectDeathInNonBlockingContext(func, "pthread_mutex_lock");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadMutexUnlockDiesWhenRealtime) {
  auto func = []() {
    auto mutex = pthread_mutex_t{};
    pthread_mutex_unlock(&mutex);
  };

  expectDeathInNonBlockingContext(func, "pthread_mutex_unlock");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadMutexJoinDiesWhenRealtime) {
  auto func = []() {
    auto thread = pthread_t{};
    pthread_join(thread, nullptr);
  };

  expectDeathInNonBlockingContext(func, "pthread_join");
  expectSurvivalInBlockableContext(func);
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
  expectDeathInNonBlockingContext(func, "OSSpinLockLock");
  expectSurvivalInBlockableContext(func);
}
#pragma clang diagnostic pop

TEST(TestRadsanInterceptors, osUnfairLockLockDiesWhenRealtime) {
  auto func = []() {
    auto unfair_lock = os_unfair_lock_s{};
    os_unfair_lock_lock(&unfair_lock);
  };
  expectDeathInNonBlockingContext(func, "os_unfair_lock_lock");
  expectSurvivalInBlockableContext(func);
}
#endif

#if SANITIZER_LINUX
TEST(TestRadsanInterceptors, spinLockLockDiesWhenRealtime) {
  auto spinlock = pthread_spinlock_t{};
  pthread_spin_init(&spinlock, PTHREAD_PROCESS_SHARED);
  auto func = [&]() { pthread_spin_lock(&spinlock); };
  expectDeathInNonBlockingContext(func, "pthread_spin_lock");
  expectSurvivalInBlockableContext(func);
}
#endif

TEST(TestRadsanInterceptors, pthreadCondSignalDiesWhenRealtime) {
  auto func = []() {
    auto cond = pthread_cond_t{};
    pthread_cond_signal(&cond);
  };
  expectDeathInNonBlockingContext(func, "pthread_cond_signal");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadCondBroadcastDiesWhenRealtime) {
  auto func = []() {
    auto cond = pthread_cond_t{};
    pthread_cond_broadcast(&cond);
  };
  expectDeathInNonBlockingContext(func, "pthread_cond_broadcast");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadCondWaitDiesWhenRealtime) {
  auto cond = pthread_cond_t{};
  auto mutex = pthread_mutex_t{};
  ASSERT_EQ(0, pthread_cond_init(&cond, nullptr));
  ASSERT_EQ(0, pthread_mutex_init(&mutex, nullptr));
  auto func = [&]() { pthread_cond_wait(&cond, &mutex); };
  expectDeathInNonBlockingContext(func, "pthread_cond_wait");
  // It's very difficult to test the success case here without doing some
  // sleeping, which is at the mercy of the scheduler. What's really important
  // here is the interception - so we're only testing that for now.
}

TEST(TestRadsanInterceptors, pthreadRwlockRdlockDiesWhenRealtime) {
  auto func = []() {
    auto rwlock = pthread_rwlock_t{};
    pthread_rwlock_rdlock(&rwlock);
  };
  expectDeathInNonBlockingContext(func, "pthread_rwlock_rdlock");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadRwlockUnlockDiesWhenRealtime) {
  auto func = []() {
    auto rwlock = pthread_rwlock_t{};
    pthread_rwlock_unlock(&rwlock);
  };
  expectDeathInNonBlockingContext(func, "pthread_rwlock_unlock");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, pthreadRwlockWrlockDiesWhenRealtime) {
  auto func = []() {
    auto rwlock = pthread_rwlock_t{};
    pthread_rwlock_wrlock(&rwlock);
  };
  expectDeathInNonBlockingContext(func, "pthread_rwlock_wrlock");
  expectSurvivalInBlockableContext(func);
}

/*
    Sockets
*/
TEST(TestRadsanInterceptors, openingASocketDiesWhenRealtime) {
  auto func = []() { socket(PF_INET, SOCK_STREAM, 0); };
  expectDeathInNonBlockingContext(func, "socket");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, sendToASocketDiesWhenRealtime) {
  auto func = []() { send(0, nullptr, 0, 0); };
  expectDeathInNonBlockingContext(func, "send");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, sendmsgToASocketDiesWhenRealtime) {
  auto const msg = msghdr{};
  auto func = [&]() { sendmsg(0, &msg, 0); };
  expectDeathInNonBlockingContext(func, "sendmsg");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, sendtoToASocketDiesWhenRealtime) {
  auto const addr = sockaddr{};
  auto const len = socklen_t{};
  auto func = [&]() { sendto(0, nullptr, 0, 0, &addr, len); };
  expectDeathInNonBlockingContext(func, "sendto");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, recvFromASocketDiesWhenRealtime) {
  auto func = []() { recv(0, nullptr, 0, 0); };
  expectDeathInNonBlockingContext(func, "recv");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, recvfromOnASocketDiesWhenRealtime) {
  auto addr = sockaddr{};
  auto len = socklen_t{};
  auto func = [&]() { recvfrom(0, nullptr, 0, 0, &addr, &len); };
  expectDeathInNonBlockingContext(func, "recvfrom");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, recvmsgOnASocketDiesWhenRealtime) {
  auto msg = msghdr{};
  auto func = [&]() { recvmsg(0, &msg, 0); };
  expectDeathInNonBlockingContext(func, "recvmsg");
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsanInterceptors, shutdownOnASocketDiesWhenRealtime) {
  auto func = [&]() { shutdown(0, 0); };
  expectDeathInNonBlockingContext(func, "shutdown");
  expectSurvivalInBlockableContext(func);
}
