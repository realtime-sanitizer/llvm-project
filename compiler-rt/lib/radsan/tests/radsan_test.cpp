#include "gtest/gtest.h"

#include "radsan_test_utilities.h"

#include <array>
#include <chrono>
#include <thread>

using namespace testing;
using namespace radsan_testing;
using namespace std::chrono_literals;

TEST(TestRadsan, vectorPushBackAllocationDiesWhenRealtime) {
  auto vec = std::vector<float>{};
  auto func = [&vec]() { vec.push_back(0.4f); };
  expectRealtimeDeath(func);
  ASSERT_EQ(0u, vec.size());
  expectNonrealtimeSurvival(func);
  ASSERT_EQ(1u, vec.size());
}

TEST(TestRadsan, destructionOfObjectOnHeapDiesWhenRealtime) {
  auto obj = std::make_unique<std::array<float, 256>>();
  auto func = [&obj]() { obj.reset(); };
  expectRealtimeDeath(func);
  ASSERT_NE(nullptr, obj.get());
  expectNonrealtimeSurvival(func);
  ASSERT_EQ(nullptr, obj.get());
}

TEST(TestRadsan, sleepingAThreadDiesWhenRealtime) {
  auto func = []() { std::this_thread::sleep_for(1us); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, fopenDiesWhenRealtime) {
  auto func = []() { fopen("./file.txt", "w"); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}
