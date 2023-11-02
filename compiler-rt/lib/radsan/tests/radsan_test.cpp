#include "gtest/gtest.h"

#include "radsan_test_utilities.h"

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
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

TEST(TestRadsan, fcloseDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "r");
  auto func = [fd]() { fclose(fd); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, ifstreamCreationDiesWhenRealtime) {
  auto func = []() { auto ifs = std::ifstream("./file.txt"); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, ofstreamCreationDiesWhenRealtime) {
  auto func = []() { auto ofs = std::ofstream("./file.txt"); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, lockingAMutexDiesWhenRealtime) {
  auto mutex = std::mutex{};
  auto func = [&]() { mutex.lock(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, unlockingAMutexDiesWhenRealtime) {
  auto mutex = std::mutex{};
  mutex.lock();
  auto func = [&]() { mutex.unlock(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, accessingALargeAtomicVariableDiesWhenRealtime) {
  auto large_atomic = std::atomic<std::array<float, 2048>>{{}};
  auto func = [&]() { auto x = large_atomic.load(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, accessingASmallAtomicVariableSurvivesWhenRealtime) {
  auto small_atomic = std::atomic<float>{0.0f};
  auto func = [&]() { auto x = small_atomic.load(); };
  realtimeInvoke(func);
}
