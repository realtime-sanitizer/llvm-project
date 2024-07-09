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

void *FakeThreadEntryPoint(void *) { return nullptr; }

class RadsanFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    file_path = std::string("/tmp/radsan_temporary_test_file_") +
                test_info->name() + ".txt";
  }

  // Gets a file path with the test's name in in
  // This file will be removed if it exists at the end of the test
  const char *GetTemporaryFilePath() const { return file_path.c_str(); }

  void TearDown() override { std::remove(GetTemporaryFilePath()); }

private:
  std::string file_path;
};

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
    void *ptr;
    posix_memalign(&ptr, 4, 4);
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

TEST_F(RadsanFileTest, OpenDiesWhenRealtime) {
  auto func = [this]() { open(GetTemporaryFilePath(), O_RDONLY); };
  ExpectRealtimeDeath(func, "open");
  ExpectNonRealtimeSurvival(func);
}

TEST_F(RadsanFileTest, OpenatDiesWhenRealtime) {
  auto func = [this]() { openat(0, GetTemporaryFilePath(), O_RDONLY); };
  ExpectRealtimeDeath(func, "openat");
  ExpectNonRealtimeSurvival(func);
}

TEST_F(RadsanFileTest, OpenCreatesFileWithProperMode) {
  const int mode = S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR;

  const int fd = open(GetTemporaryFilePath(), O_CREAT | O_WRONLY, mode);
  ASSERT_THAT(fd, Ne(-1));
  close(fd);

  struct stat st;
  ASSERT_THAT(stat(GetTemporaryFilePath(), &st), Eq(0));

  // Mask st_mode to get permission bits only
  ASSERT_THAT(st.st_mode & 0777, Eq(mode));
}

TEST_F(RadsanFileTest, CreatDiesWhenRealtime) {
  auto func = [this]() { creat(GetTemporaryFilePath(), S_IWOTH | S_IROTH); };
  ExpectRealtimeDeath(func, "creat");
  ExpectNonRealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, FcntlDiesWhenRealtime) {
  auto func = []() { fcntl(0, F_GETFL); };
  ExpectRealtimeDeath(func, "fcntl");
  ExpectNonRealtimeSurvival(func);
}

TEST_F(RadsanFileTest, FcntlFlockDiesWhenRealtime) {
  int fd = creat(GetTemporaryFilePath(), S_IRUSR | S_IWUSR);
  ASSERT_THAT(fd, Ne(-1));

  auto func = [fd]() {
    struct flock lock {};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = ::getpid();

    ASSERT_THAT(fcntl(fd, F_GETLK, &lock), Eq(0));
    ASSERT_THAT(lock.l_type, F_UNLCK);
  };
  ExpectRealtimeDeath(func, "fcntl");
  ExpectNonRealtimeSurvival(func);

  close(fd);
}

TEST_F(RadsanFileTest, FcntlSetFdDiesWhenRealtime) {
  int fd = creat(GetTemporaryFilePath(), S_IRUSR | S_IWUSR);
  ASSERT_THAT(fd, Ne(-1));

  auto func = [fd]() {
    int old_flags = fcntl(fd, F_GETFD);
    ASSERT_THAT(fcntl(fd, F_SETFD, FD_CLOEXEC), Eq(0));

    int flags = fcntl(fd, F_GETFD);
    ASSERT_THAT(flags, Ne(-1));
    ASSERT_THAT(flags & FD_CLOEXEC, Eq(FD_CLOEXEC));

    ASSERT_THAT(fcntl(fd, F_SETFD, old_flags), Eq(0));
    ASSERT_THAT(fcntl(fd, F_GETFD), Eq(old_flags));
  };

  ExpectRealtimeDeath(func, "fcntl");
  ExpectNonRealtimeSurvival(func);

  close(fd);
}

TEST(TestRadsanInterceptors, CloseDiesWhenRealtime) {
  auto func = []() { close(0); };
  ExpectRealtimeDeath(func, "close");
  ExpectNonRealtimeSurvival(func);
}

TEST_F(RadsanFileTest, FopenDiesWhenRealtime) {
  auto func = [this]() {
    auto fd = fopen(GetTemporaryFilePath(), "w");
    EXPECT_THAT(fd, Ne(nullptr));
  };
  ExpectRealtimeDeath(func, "fopen");
  ExpectNonRealtimeSurvival(func);
}

TEST_F(RadsanFileTest, FreadDiesWhenRealtime) {
  auto fd = fopen(GetTemporaryFilePath(), "w");
  auto func = [fd]() {
    char c{};
    fread(&c, 1, 1, fd);
  };
  ExpectRealtimeDeath(func, "fread");
  ExpectNonRealtimeSurvival(func);
  if (fd != nullptr)
    fclose(fd);
}

TEST_F(RadsanFileTest, FwriteDiesWhenRealtime) {
  auto fd = fopen(GetTemporaryFilePath(), "w");
  ASSERT_NE(nullptr, fd);
  auto message = "Hello, world!";
  auto func = [&]() { fwrite(&message, 1, 4, fd); };
  ExpectRealtimeDeath(func, "fwrite");
  ExpectNonRealtimeSurvival(func);
}

TEST_F(RadsanFileTest, FcloseDiesWhenRealtime) {
  auto fd = fopen(GetTemporaryFilePath(), "w");
  EXPECT_THAT(fd, Ne(nullptr));
  auto func = [fd]() { fclose(fd); };
  ExpectRealtimeDeath(func, "fclose");
  ExpectNonRealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, PutsDiesWhenRealtime) {
  auto func = []() { puts("Hello, world!\n"); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST_F(RadsanFileTest, FputsDiesWhenRealtime) {
  auto fd = fopen(GetTemporaryFilePath(), "w");
  ASSERT_THAT(fd, Ne(nullptr)) << errno;
  auto func = [fd]() { fputs("Hello, world!\n", fd); };
  ExpectRealtimeDeath(func);
  ExpectNonRealtimeSurvival(func);
  if (fd != nullptr)
    fclose(fd);
}

/*
    Concurrency
*/

TEST(TestRadsanInterceptors, PthreadCreateDiesWhenRealtime) {
  auto Func = []() {
    pthread_t thread{};
    const pthread_attr_t attr{};
    struct thread_info *thread_info;
    pthread_create(&thread, &attr, &FakeThreadEntryPoint, thread_info);
  };
  expectRealtimeDeath(Func, "pthread_create");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadMutexLockDiesWhenRealtime) {
  auto Func = []() {
    pthread_mutex_t mutex{};
    pthread_mutex_lock(&mutex);
  };

  expectRealtimeDeath(Func, "pthread_mutex_lock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadMutexUnlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_mutex_t mutex{};
    pthread_mutex_unlock(&mutex);
  };

  expectRealtimeDeath(Func, "pthread_mutex_unlock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadMutexJoinDiesWhenRealtime) {
  auto Func = []() {
    pthread_t thread{};
    pthread_join(thread, nullptr);
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
    OSSpinLock spin_lock{};
    OSSpinLockLock(&spin_lock);
  };
  expectRealtimeDeath(Func, "OSSpinLockLock");
  expectNonrealtimeSurvival(Func);
}
#pragma clang diagnostic pop

TEST(TestRadsanInterceptors, OsUnfairLockLockDiesWhenRealtime) {
  auto Func = []() {
    os_unfair_lock_s unfair_lock{};
    os_unfair_lock_lock(&unfair_lock);
  };
  expectRealtimeDeath(Func, "os_unfair_lock_lock");
  expectNonrealtimeSurvival(Func);
}
#endif

#if SANITIZER_LINUX
TEST(TestRadsanInterceptors, SpinLockLockDiesWhenRealtime) {
  pthread_spinlock_t spin_lock;
  pthread_spin_init(&spin_lock, PTHREAD_PROCESS_SHARED);
  auto Func = [&]() { pthread_spin_lock(&spin_lock); };
  expectRealtimeDeath(Func, "pthread_spin_lock");
  expectNonrealtimeSurvival(Func);
}
#endif

TEST(TestRadsanInterceptors, PthreadCondSignalDiesWhenRealtime) {
  pthread_cond_t cond{};
  pthread_cond_init(&cond, NULL);

  auto Func = [&cond]() { pthread_cond_signal(&cond); };
  expectRealtimeDeath(Func, "pthread_cond_signal");
  expectNonrealtimeSurvival(Func);

  pthread_cond_destroy(&cond);
}

TEST(TestRadsanInterceptors, PthreadCondBroadcastDiesWhenRealtime) {
  pthread_cond_t cond{};
  pthread_cond_init(&cond, NULL);

  auto Func = [&cond]() { pthread_cond_broadcast(&cond); };
  expectRealtimeDeath(Func, "pthread_cond_broadcast");
  expectNonrealtimeSurvival(Func);

  pthread_cond_destroy(&cond);
}

TEST(TestRadsanInterceptors, PthreadCondWaitDiesWhenRealtime) {
  pthread_cond_t cond;
  pthread_mutex_t mutex;
  ASSERT_EQ(0, pthread_cond_init(&cond, nullptr));
  ASSERT_EQ(0, pthread_mutex_init(&mutex, nullptr));

  auto Func = [&]() { pthread_cond_wait(&cond, &mutex); };
  expectRealtimeDeath(Func, "pthread_cond_wait");
  // It's very difficult to test the success case here without doing some
  // sleeping, which is at the mercy of the scheduler. What's really important
  // here is the interception - so we're only testing that for now.

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex);
}

TEST(TestRadsanInterceptors, PthreadRwlockRdlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_rwlock_t rw_lock;
    pthread_rwlock_rdlock(&rw_lock);
  };
  expectRealtimeDeath(Func, "pthread_rwlock_rdlock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadRwlockUnlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_rwlock_t rw_lock;
    pthread_rwlock_unlock(&rw_lock);
  };
  expectRealtimeDeath(Func, "pthread_rwlock_unlock");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, PthreadRwlockWrlockDiesWhenRealtime) {
  auto Func = []() {
    pthread_rwlock_t rw_lock;
    pthread_rwlock_wrlock(&rw_lock);
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
  msghdr msg{};
  auto Func = [&]() { sendmsg(0, &msg, 0); };
  expectRealtimeDeath(Func, "sendmsg");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, SendtoToASocketDiesWhenRealtime) {
  sockaddr addr{};
  socklen_t len{};
  auto Func = [&]() { sendto(0, nullptr, 0, 0, &addr, len); };
  expectRealtimeDeath(Func, "sendto");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, RecvFromASocketDiesWhenRealtime) {
  auto Func = []() { recv(0, nullptr, 0, 0); };
  expectRealtimeDeath(Func, "recv");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, RecvfromOnASocketDiesWhenRealtime) {
  sockaddr addr{};
  socklen_t len{};
  auto Func = [&]() { recvfrom(0, nullptr, 0, 0, &addr, &len); };
  expectRealtimeDeath(Func, "recvfrom");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, RecvmsgOnASocketDiesWhenRealtime) {
  msghdr msg{};
  auto Func = [&]() { recvmsg(0, &msg, 0); };
  expectRealtimeDeath(Func, "recvmsg");
  expectNonrealtimeSurvival(Func);
}

TEST(TestRadsanInterceptors, ShutdownOnASocketDiesWhenRealtime) {
  auto Func = [&]() { shutdown(0, 0); };
  expectRealtimeDeath(Func, "shutdown");
  expectNonrealtimeSurvival(Func);
}
