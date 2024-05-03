/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "gmock/gmock.h"

#include "radsan_test_utilities.h"
#include <radsan.h>
#include <sanitizer_common/sanitizer_platform.h>
#include <sanitizer_common/sanitizer_platform_interceptors.h>

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <thread>

#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) &&                  \
    __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101200
#define SI_MAC_DEPLOYMENT_AT_LEAST_10_12 1
#else
#define SI_MAC_DEPLOYMENT_AT_LEAST_10_12 0
#endif

#define RADSAN_TEST_SHARED_MUTEX (!(SI_MAC) || SI_MAC_DEPLOYMENT_AT_LEAST_10_12)

using namespace testing;
using namespace radsan_testing;
using namespace std::chrono_literals;

namespace {
void invokeStdFunction(std::function<void()> &&function) { function(); }
} // namespace

TEST(TestRadsan, vectorPushBackAllocationDiesWhenRealtime) {
  auto vec = std::vector<float>{};
  auto func = [&vec]() { vec.push_back(0.4f); };
  expectDeathInNonBlockingContext(func);
  ASSERT_EQ(0u, vec.size());
  expectSurvivalInBlockableContext(func);
  ASSERT_THAT(vec.size(), Gt(0u));
}

TEST(TestRadsan, destructionOfObjectOnHeapDiesWhenRealtime) {
  auto obj = std::make_unique<std::array<float, 256>>();
  auto func = [&obj]() { obj.reset(); };
  expectDeathInNonBlockingContext(func);
  ASSERT_NE(nullptr, obj.get());
  expectSurvivalInBlockableContext(func);
  ASSERT_EQ(nullptr, obj.get());
}

TEST(TestRadsan, sleepingAThreadDiesWhenRealtime) {
  auto func = []() { std::this_thread::sleep_for(1us); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, ifstreamCreationDiesWhenRealtime) {
  auto func = []() { auto ifs = std::ifstream("./file.txt"); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
  std::remove("./file.txt");
}

TEST(TestRadsan, ofstreamCreationDiesWhenRealtime) {
  auto func = []() { auto ofs = std::ofstream("./file.txt"); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
  std::remove("./file.txt");
}

TEST(TestRadsan, lockingAMutexDiesWhenRealtime) {
  auto mutex = std::mutex{};
  auto func = [&]() { mutex.lock(); };
  expectDeathInNonBlockingContext(func);
  auto reset = [&]() { mutex.unlock(); };
  expectSurvivalInBlockableContext(func, reset);
}

TEST(TestRadsan, unlockingAMutexDiesWhenRealtime) {
  auto mutex = std::mutex{};
  mutex.lock();
  auto func = [&]() { mutex.unlock(); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

#if RADSAN_TEST_SHARED_MUTEX

TEST(TestRadsan, lockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  auto func = [&]() { mutex.lock(); };
  expectDeathInNonBlockingContext(func);
  auto reset = [&]() { mutex.unlock(); };
  expectSurvivalInBlockableContext(func, reset);
}

TEST(TestRadsan, unlockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  mutex.lock();
  auto func = [&]() { mutex.unlock(); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, sharedLockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  auto func = [&]() { mutex.lock_shared(); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, sharedUnlockingASharedMutexDiesWhenRealtime) {
  auto mutex = std::shared_mutex();
  mutex.lock_shared();
  auto func = [&]() { mutex.unlock_shared(); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

#endif // RADSAN_TEST_SHARED_MUTEX

TEST(TestRadsan, launchingAThreadDiesWhenRealtime) {
  auto func = [&]() {
    auto t = std::thread([]() {});
    t.join();
  };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
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
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, accessingALargeAtomicVariableDiesWhenRealtime) {
  auto small_atomic = std::atomic<float>{0.0f};
  ASSERT_TRUE(small_atomic.is_lock_free());
  expectSurvivalInNonBlockingContext([&small_atomic]() { auto x = small_atomic.load(); });

  auto large_atomic = std::atomic<std::array<float, 2048>>{{}};
  ASSERT_FALSE(large_atomic.is_lock_free());
  auto func = [&]() { auto x = large_atomic.load(); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, firstCoutDiesWhenRealtime) {
  auto func = []() { std::cout << "Hello, world!" << std::endl; };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, secondCoutDiesWhenRealtime) {
  std::cout << "Hello, world";
  auto func = []() { std::cout << "Hello, again!" << std::endl; };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, printfDiesWhenRealtime) {
  auto func = []() { printf("Hello, world!\n"); };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, throwingAnExceptionDiesWhenRealtime) {
  auto func = [&]() {
    try {
      throw std::exception();
    } catch (std::exception &) {
    }
  };
  expectDeathInNonBlockingContext(func);
  expectSurvivalInBlockableContext(func);
}

TEST(TestRadsan, doesNotDieIfTurnedOff) {
  auto mutex = std::mutex{};
  auto realtime_unsafe_func = [&]() {
    radsan_off();
    mutex.lock();
    mutex.unlock();
    radsan_on();
  };
  expectSurvivalInNonBlockingContext(realtime_unsafe_func);
}
