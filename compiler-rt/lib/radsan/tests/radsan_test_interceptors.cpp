//===--- radsan_test_interceptors.cpp - Realtime Sanitizer --------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

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
#include <string>
#include <thread>

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>

using namespace testing;
using namespace radsan_testing;
using namespace std::chrono_literals;

namespace {
void *FakeThreadEntryPoint(void *) { return nullptr; }

/*
  The creat function doesn't seem to work on an ubuntu Docker image when the
  path is in a shared volume of the host. For now, to keep testing convenient
  with a local Docker container, we just put it somewhere that's not in the
  shared volume (/tmp). This is volatile and will be cleaned up as soon as the
  container is stopped.
*/
constexpr const char *TemporaryFilePath() {
#if SANITIZER_LINUX
  return "/tmp/radsan_temporary_test_file.txt";
#elif SANITIZER_APPLE
  return "./radsan_temporary_test_file.txt";
#endif
}
} // namespace

class RadsanFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    file_path_ = std::string("/tmp/radsan_temporary_test_file_") +
                 test_info->name() + ".txt";
    RemoveTemporaryFile();
  }

  // Gets a file path with the test's name in it
  // This file will be removed if it exists at the end of the test
  const char *GetTemporaryFilePath() const { return file_path_.c_str(); }

  void TearDown() override { RemoveTemporaryFile(); }

private:
  void RemoveTemporaryFile() const { std::remove(GetTemporaryFilePath()); }
  std::string file_path_;
};

/*
    Allocation and deallocation
*/

TEST(TestRadsanInterceptors, MallocDiesWhenRealtime) {
  auto Func = []() { EXPECT_NE(nullptr, malloc(1)); };
  expectRealtimeDeath(Func, "malloc");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, ReallocDiesWhenRealtime) {
  void *ptr_1 = malloc(1);
  auto Func = [ptr_1]() { EXPECT_NE(nullptr, realloc(ptr_1, 8)); };
  expectRealtimeDeath(Func, "realloc");
  expectNonrealtimeSurvival(Func);
}

#if SANITIZER_APPLE
TEST(TestRadsanInterceptors, ReallocfDiesWhenRealtime) {
  void *ptr_1 = malloc(1);
  auto Func = [ptr_1]() { EXPECT_NE(nullptr, reallocf(ptr_1, 8)); };
  expectRealtimeDeath(Func, "reallocf");
  expectNonrealtimeSurvival(Func);
}
#endif

TEST(TestRadsanInterceptors, VallocDiesWhenRealtime) {
  auto Func = []() { EXPECT_NE(nullptr, valloc(4)); };
  expectRealtimeDeath(Func, "valloc");
  expectNonrealtimeSurvival(Func);
}

#if SANITIZER_INTERCEPT_ALIGNED_ALLOC
TEST(TestRadsanInterceptors, AlignedAllocDiesWhenRealtime) {
  auto Func = []() { EXPECT_NE(nullptr, aligned_alloc(16, 32)); };
  expectRealtimeDeath(Func, "aligned_alloc");
  expectNonrealtimeSurvival(Func);
}
#endif

// free_sized and free_aligned_sized (both C23) are not yet supported
TEST(TestRadsanInterceptors, FreeDiesWhenRealtime) {
  void *ptr_1 = malloc(1);
  void *ptr_2 = malloc(1);
  expectRealtimeDeath([ptr_1]() { free(ptr_1); }, "free");
  expectNonrealtimeSurvival([ptr_2]() { free(ptr_2); });

  // Prevent malloc/free pair being optimised out
  ASSERT_NE(nullptr, ptr_1);
  ASSERT_NE(nullptr, ptr_2);
}

TEST(TestRadsanInterceptors, FreeSurvivesWhenRealtimeIfArgumentIsNull) {
  RealtimeInvoke([]() { free(NULL); });
  expectNonrealtimeSurvival([]() { free(NULL); });
}

TEST(TestRadsanInterceptors, PosixMemalignDiesWhenRealtime) {
  auto Func = []() {
    void *Mem;
    posix_memalign(&Mem, 4, 4);
  };
  expectRealtimeDeath(Func, "posix_memalign");
  expectNonrealtimeSurvival(Func);
}

#if SANITIZER_INTERCEPT_MEMALIGN
TEST(TestRadsanInterceptors, MemalignDiesWhenRealtime) {
  auto Func = []() { EXPECT_NE(memalign(2, 2048), nullptr); };
  expectRealtimeDeath(Func, "memalign");
  expectNonrealtimeSurvival(Func);
}
#endif

#if SANITIZER_INTERCEPT_PVALLOC
TEST(TestRadsanInterceptors, PvallocDiesWhenRealtime) {
  auto Func = []() { EXPECT_NE(pvalloc(2048), nullptr); };
  expectRealtimeDeath(Func, "pvalloc");
  expectNonrealtimeSurvival(Func);
}
#endif

/*
    Sleeping
*/

TEST(TestRadsanInterceptors, SleepDiesWhenRealtime) {
  auto Func = []() { sleep(0u); };
  expectRealtimeDeath(Func, "sleep");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, UsleepDiesWhenRealtime) {
  auto Func = []() { usleep(1u); };
  expectRealtimeDeath(Func, "usleep");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, NanosleepDiesWhenRealtime) {
  auto Func = []() {
    timespec T{};
    nanosleep(&T, &T);
  };
  expectRealtimeDeath(Func, "nanosleep");
  expectNonrealtimeSurvival(Func);
}

/*
    Filesystem
*/

TEST(TestRadsanInterceptors, OpenDiesWhenRealtime) {
  auto Func = []() { open(TemporaryFilePath(), O_RDONLY); };
  ExpectRealtimeDeath(Func, "open");
  ExpectNonRealtimeSurvival(Func);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, OpenatDiesWhenRealtime) {
  auto Func = []() { openat(0, TemporaryFilePath(), O_RDONLY); };
  ExpectRealtimeDeath(Func, "openat");
  ExpectNonRealtimeSurvival(Func);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, CreatDiesWhenRealtime) {
  auto Func = []() { creat(TemporaryFilePath(), S_IWOTH | S_IROTH); };
  ExpectRealtimeDeath(Func, "creat");
  ExpectNonRealtimeSurvival(Func);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, FcntlDiesWhenRealtime) {
  auto Func = []() { fcntl(0, F_GETFL); };
  ExpectRealtimeDeath(Func, "fcntl");
  ExpectNonRealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, CloseDiesWhenRealtime) {
  auto Func = []() { close(0); };
  expectRealtimeDeath(Func, "close");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, FopenDiesWhenRealtime) {
  auto Func = []() {
    FILE *Fd = fopen(TemporaryFilePath(), "w");
    EXPECT_THAT(Fd, Ne(nullptr));
  };
  ExpectRealtimeDeath(Func, "fopen");
  ExpectNonRealtimeSurvival(Func);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, FreadDiesWhenRealtime) {
  FILE *Fd = fopen(TemporaryFilePath(), "w");
  auto Func = [Fd]() {
    char c{};
    fread(&c, 1, 1, Fd);
  };
  ExpectRealtimeDeath(Func, "fread");
  ExpectNonRealtimeSurvival(Func);
  if (Fd != nullptr)
    fclose(Fd);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, FwriteDiesWhenRealtime) {
  FILE *Fd = fopen(TemporaryFilePath(), "w");
  ASSERT_NE(nullptr, Fd);
  const char *Message = "Hello, world!";
  auto Func = [&]() { fwrite(&Message, 1, 4, Fd); };
  ExpectRealtimeDeath(Func, "fwrite");
  ExpectNonRealtimeSurvival(Func);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, FcloseDiesWhenRealtime) {
  FILE *Fd = fopen(TemporaryFilePath(), "w");
  EXPECT_THAT(Fd, Ne(nullptr));
  auto Func = [Fd]() { fclose(Fd); };
  ExpectRealtimeDeath(Func, "fclose");
  ExpectNonRealtimeSurvival(Func);
  std::remove(TemporaryFilePath());
}

TEST(TestRadsanInterceptors, PutsDiesWhenRealtime) {
  auto Func = []() { puts("Hello, world!\n"); };
  expectRealtimeDeath(Func);
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, FputsDiesWhenRealtime) {
  FILE *Fd = fopen(TemporaryFilePath(), "w");
  ASSERT_THAT(Fd, Ne(nullptr)) << errno;
  auto Func = [Fd]() { fputs("Hello, world!\n", Fd); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
  if (Fd != nullptr)
    fclose(Fd);
  std::remove(TemporaryFilePath());
}

/*
    Concurrency
*/

TEST(TestRadsanInterceptors, PthreadCreateDiesWhenRealtime) {
  auto Func = []() {
    pthread_t Thread{};
    const pthread_attr_t Attr{};
    struct thread_info *ThreadInfo;
    pthread_create(&Thread, &Attr, &FakeThreadEntryPoint, ThreadInfo);
  };
  expectRealtimeDeath(Func, "pthread_create");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadMutexLockDiesWhenRealtime) {
  auto Func = []() {
    pthread_mutex_t Mutex{};
    pthread_mutex_lock(&Mutex);
  };

  expectRealtimeDeath(Func, "pthread_mutex_lock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadMutexUnlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_mutex_t Mutex{};
    pthread_mutex_unlock(&Mutex);
  };

  expectRealtimeDeath(Func, "pthread_mutex_unlock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadMutexJoinDiesWhenRealtime) {
  auto Func = []() {
    pthread_t Thread{};
    pthread_join(Thread, nullptr);
  };

  expectRealtimeDeath(Func, "pthread_join");
  expectNonrealtimeSurvival(Func);
}

#if SANITIZER_APPLE

#pragma clang diagnostic push
// OSSpinLockLock is deprecated, but still in use in libc++
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST(TestRadsanInterceptors, OsSpinLockLockDiesWhenRealtime) {
  auto Func = []() {
    OSSpinLock SpinLock{};
    OSSpinLockLock(&SpinLock);
  };
  expectRealtimeDeath(Func, "OSSpinLockLock");
  expectNonrealtimeSurvival(Func);
}
#pragma clang diagnostic pop

TEST(TestRadsanInterceptors, OsUnfairLockLockDiesWhenRealtime) {
  auto Func = []() {
    os_unfair_lock_s UnfairLock{};
    os_unfair_lock_lock(&UnfairLock);
  };
  expectRealtimeDeath(Func, "os_unfair_lock_lock");
  expectNonrealtimeSurvival(Func);
}
#endif

#if SANITIZER_LINUX
TEST(TestRadsanInterceptors, SpinLockLockDiesWhenRealtime) {
  pthread_spinlock_t SpinLock;
  pthread_spin_init(&SpinLock, PTHREAD_PROCESS_SHARED);
  auto Func = [&]() { pthread_spin_lock(&SpinLock); };
  expectRealtimeDeath(Func, "pthread_spin_lock");
  expectNonrealtimeSurvival(Func);
}
#endif

TEST(TestRadsanInterceptors, PthreadCondSignalDiesWhenRealtime) {
  auto Func = []() {
    pthread_cond_t Cond{};
    pthread_cond_signal(&Cond);
  };
  expectRealtimeDeath(Func, "pthread_cond_signal");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadCondBroadcastDiesWhenRealtime) {
  auto Func = []() {
    pthread_cond_t Cond;
    pthread_cond_broadcast(&Cond);
  };
  expectRealtimeDeath(Func, "pthread_cond_broadcast");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadCondWaitDiesWhenRealtime) {
  pthread_cond_t Cond;
  pthread_mutex_t Mutex;
  ASSERT_EQ(0, pthread_cond_init(&Cond, nullptr));
  ASSERT_EQ(0, pthread_mutex_init(&Mutex, nullptr));
  auto Func = [&]() { pthread_cond_wait(&Cond, &Mutex); };
  expectRealtimeDeath(Func, "pthread_cond_wait");
  // It's very difficult to test the success case here without doing some
  // sleeping, which is at the mercy of the scheduler. What's really important
  // here is the interception - so we're only testing that for now.
}

TEST(TestRadsanInterceptors, PthreadRwlockRdlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_rwlock_t RwLock;
    pthread_rwlock_rdlock(&RwLock);
  };
  expectRealtimeDeath(Func, "pthread_rwlock_rdlock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadRwlockUnlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_rwlock_t RwLock;
    pthread_rwlock_unlock(&RwLock);
  };
  expectRealtimeDeath(Func, "pthread_rwlock_unlock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadRwlockWrlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_rwlock_t RwLock;
    pthread_rwlock_wrlock(&RwLock);
  };
  expectRealtimeDeath(Func, "pthread_rwlock_wrlock");
  expectNonrealtimeSurvival(Func);
}

/*
    Sockets
*/
TEST(TestRadsanInterceptors, OpeningASocketDiesWhenRealtime) {
  auto Func = []() { socket(PF_INET, SOCK_STREAM, 0); };
  expectRealtimeDeath(Func, "socket");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, SendToASocketDiesWhenRealtime) {
  auto Func = []() { send(0, nullptr, 0, 0); };
  expectRealtimeDeath(Func, "send");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, SendmsgToASocketDiesWhenRealtime) {
  msghdr Msg{};
  auto Func = [&]() { sendmsg(0, &Msg, 0); };
  expectRealtimeDeath(Func, "sendmsg");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, SendtoToASocketDiesWhenRealtime) {
  sockaddr Addr{};
  socklen_t Len{};
  auto Func = [&]() { sendto(0, nullptr, 0, 0, &Addr, Len); };
  expectRealtimeDeath(Func, "sendto");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, RecvFromASocketDiesWhenRealtime) {
  auto Func = []() { recv(0, nullptr, 0, 0); };
  expectRealtimeDeath(Func, "recv");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, RecvfromOnASocketDiesWhenRealtime) {
  sockaddr Addr{};
  socklen_t Len{};
  auto Func = [&]() { recvfrom(0, nullptr, 0, 0, &Addr, &Len); };
  expectRealtimeDeath(Func, "recvfrom");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, RecvmsgOnASocketDiesWhenRealtime) {
  msghdr Msg{};
  auto Func = [&]() { recvmsg(0, &Msg, 0); };
  expectRealtimeDeath(Func, "recvmsg");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, ShutdownOnASocketDiesWhenRealtime) {
  auto Func = [&]() { shutdown(0, 0); };
  expectRealtimeDeath(Func, "shutdown");
  expectNonrealtimeSurvival(Func);
}
