/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "gtest/gtest.h"

#include "radsan_test_utilities.h"
#include <radsan.h>
#include <sanitizer_common/sanitizer_platform.h>

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <thread>

using namespace testing;
using namespace radsan_testing;
using namespace std::chrono_literals;

namespace {
void invokeStdFunction(std::function<void()> &&function) { function(); }
} // namespace

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
  auto func = []() {
    auto fd = fopen("./file.txt", "w");
    EXPECT_THAT(fd, Ne(nullptr));
    if (fd != nullptr)
      fclose(fd);
  };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, fcloseDiesWhenRealtime) {
  auto fd = fopen("./file.txt", "r");
  ASSERT_THAT(fd, Ne(nullptr));
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

TEST(TestRadsan, lockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  auto func = [&]() { mutex.lock(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, unlockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  mutex.lock();
  auto func = [&]() { mutex.unlock(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, sharedLockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  auto func = [&]() { mutex.lock_shared(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, sharedUnlockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  mutex.lock_shared();
  auto func = [&]() { mutex.unlock_shared(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, launchingAThreadDiesWhenRealtime) {
  auto func = [&]() {
    auto t = std::thread([]() {});
    t.join();
  };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, copyingALambdaWithLargeCaptureDiesWhenRealtime) {
  auto lots_of_data = std::array<float, 16>{};
  auto lambda = [lots_of_data]() mutable {
    // Stop everything getting optimised out
    lots_of_data[3] = 0.25f;
    EXPECT_EQ(16, lots_of_data.size());
    EXPECT_EQ(0.25f, lots_of_data[3]);
  };
  auto func = [&]() { invokeStdFunction(lambda); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, accessingALargeAtomicVariableDiesWhenRealtime) {
  auto small_atomic = std::atomic<float>{0.0f};
  ASSERT_TRUE(small_atomic.is_lock_free());
  realtimeInvoke([&small_atomic]() { auto x = small_atomic.load(); });

  auto large_atomic = std::atomic<std::array<float, 2048>>{{}};
  ASSERT_FALSE(large_atomic.is_lock_free());
  auto func = [&]() { auto x = large_atomic.load(); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, firstCoutDiesWhenRealtime) {
  auto func = []() { std::cout << "Hello, world!" << std::endl; };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, secondCoutDiesWhenRealtime) {
  std::cout << "Hello, world";
  auto func = []() { std::cout << "Hello, again!" << std::endl; };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, printfDiesWhenRealtime) {
  auto func = []() { printf("Hello, world!\n"); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, throwingAnExceptionDiesWhenRealtime) {
  auto func = [&]() {
    try {
      throw std::exception();
    } catch (std::exception &) {
    }
  };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, doesNotDieIfTurnedOff) {
  auto mutex = std::mutex{};
  auto realtime_unsafe_func = [&]() {
    radsan_off();
    mutex.lock();
    mutex.unlock();
    radsan_on();
  };
  realtimeInvoke(realtime_unsafe_func);
}
