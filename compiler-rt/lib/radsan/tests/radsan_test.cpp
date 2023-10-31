#include "gtest/gtest.h"

#include <array>

using namespace testing;

namespace {
template <typename Function>
[[clang::realtime]] void realtimeInvoke(Function &&func) {
  std::forward<Function>(func)();
}
template <typename Function> void expectRealtimeDeath(Function &&func) {
  EXPECT_DEATH(realtimeInvoke(std::forward<Function>(func)), "radsan");
}

template <typename Function> void expectNonrealtimeSurvival(Function &&func) {
  std::forward<Function>(func)();
}
} // namespace

TEST(TestRealtimeSanitizer, mallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, malloc(1)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRealtimeSanitizer, reallocDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, realloc(ptr_1, 8)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRealtimeSanitizer, reallocfDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto func = [ptr_1]() { EXPECT_NE(nullptr, reallocf(ptr_1, 8)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRealtimeSanitizer, vallocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, valloc(4)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRealtimeSanitizer, alignedAllocDiesWhenRealtime) {
  auto func = []() { EXPECT_NE(nullptr, aligned_alloc(16, 32)); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

/*
    free_sized and free_aligned_sized (both C23) are not yet supported
*/
TEST(TestRealtimeSanitizer, freeDiesWhenRealtime) {
  auto *ptr_1 = malloc(1);
  auto *ptr_2 = malloc(1);
  expectRealtimeDeath([ptr_1]() { free(ptr_1); });
  expectNonrealtimeSurvival([ptr_2]() { free(ptr_2); });

  // Prevent malloc/free pair being optimised out
  ASSERT_NE(nullptr, ptr_1);
  ASSERT_NE(nullptr, ptr_2);
}

TEST(TestRealtimeSanitizer, vectorPushBackAllocationDiesWhenRealtime) {
  auto vec = std::vector<float>{};
  auto func = [&vec]() { vec.push_back(0.4f); };
  expectRealtimeDeath(func);
  ASSERT_EQ(0u, vec.size());
  expectNonrealtimeSurvival(func);
  ASSERT_EQ(1u, vec.size());
}

TEST(TestRealtimeSanitizer, destructionOfObjectOnHeapDiesWhenRealtime) {
  auto obj = std::make_unique<std::array<float, 256>>();
  auto func = [&obj]() { obj.reset(); };
  expectRealtimeDeath(func);
  ASSERT_NE(nullptr, obj.get());
  expectNonrealtimeSurvival(func);
  ASSERT_EQ(nullptr, obj.get());
}
