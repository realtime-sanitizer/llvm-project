#include "gtest/gtest.h"

#include "radsan_test_utilities.h"

#include <fcntl.h>

using namespace testing;
using namespace radsan_testing;

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
