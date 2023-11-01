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
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, reallocDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, realloc(ptr_1, 8)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, reallocfDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, reallocf(ptr_1, 8)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, vallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, valloc(4)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, alignedAllocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, aligned_alloc(16, 32)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

// free_sized and free_aligned_sized (both C23) are not yet supported
TEST(TestRadsanInterceptors, freeDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto *ptr_2 = malloc(1);
  expectRealtimeDeath([ptr_1]() { free(ptr_1); });
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
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, usleepDiesWhenRealtime) {
  auto func = []() { usleep(1u); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, nanosleepDiesWhenRealtime) {
  auto func = []() {
    auto t = timespec{};
    nanosleep(&t, &t);
  };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

/*
    Filesystem system calls
*/
TEST(TestRadsanInterceptors, openDiesWhenRealtime) {
  auto func = []() { open("./file.txt", O_RDONLY); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsanInterceptors, creatDiesWhenRealtime) {
  auto func = []() { creat("./file.txt", O_TRUNC); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}
