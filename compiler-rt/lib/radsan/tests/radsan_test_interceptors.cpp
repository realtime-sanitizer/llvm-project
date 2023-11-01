#include "gtest/gtest.h"

#include "radsan_test_utilities.h"

#include <fcntl.h>
#include <stdio.h>

using namespace testing;
using namespace radsan_testing;

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

TEST(TestRadsanInterceptors, reallocfDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, reallocf(ptr_1, 8)); };
  expectRealtimeDeath(func, "reallocf");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, vallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, valloc(4)); };
  expectRealtimeDeath(func, "valloc");
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, alignedAllocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, aligned_alloc(16, 32)); };
  expectRealtimeDeath(func, "aligned_alloc");
  expectNonrealtimeSurvival(func);
}

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
    Filesystem system calls
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

TEST(TestRadsanInterceptors, fcloseDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "r");
  auto func = [fd]() { fclose(fd); };
  expectRealtimeDeath(func, "fclose");
  expectNonrealtimeSurvival(func);
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

/*


TODO: maybe

pthread_spin_lock
pthread_spin_unlock?????

pthread_cond_signal
pthread_cond_broadcast

pthread_cond_wait
pthread_cond_timed_wait

pthread_rwlock_rdlock
pthread_rwlock_unlock

pthread_rwlock_wrlock

*/