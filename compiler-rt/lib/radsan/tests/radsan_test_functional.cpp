//===--- radsan_test.cpp - Realtime Sanitizer --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Introduces basic functional tests for the realtime sanitizer.
// Not meant to be exhaustive, testing all interceptors, please see
// test_radsan_interceptors.cpp for those tests.
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

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

TEST(TestRadsan, VectorPushBackAllocationDiesWhenRealtime) {
  std::vector<float> vec{};
  auto Func = [&vec]() { vec.push_back(0.4f); };
  ExpectRealtimeDeath(Func);
  ASSERT_EQ(0u, vec.size());
  ExpectNonRealtimeSurvival(Func);
  ASSERT_EQ(1u, vec.size());
}

TEST(TestRadsan, DestructionOfObjectOnHeapDiesWhenRealtime) {
  auto allocated_ptr = std::make_unique<std::array<float, 256>>();
  auto Func = [&allocated_ptr]() { allocated_ptr.reset(); };
  ExpectRealtimeDeath(Func);
  ASSERT_NE(nullptr, allocated_ptr.get());
  ExpectNonRealtimeSurvival(Func);
  ASSERT_EQ(nullptr, allocated_ptr.get());
}

TEST(TestRadsan, sleepingAThreadDiesWhenRealtime) {
  auto func = []() { std::this_thread::sleep_for(1us); };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

TEST(TestRadsan, ifstreamCreationDiesWhenRealtime) {
  auto func = []() { std::ifstream ifs{"./file.txt"}; };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
  std::remove("./file.txt");
}

TEST(TestRadsan, ofstreamCreationDiesWhenRealtime) {
  auto func = []() { std::ofstream ofs{"./file.txt"}; };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
  std::remove("./file.txt");
}

TEST(TestRadsan, LockingAMutexDiesWhenRealtime) {
  std::mutex mutex{};
  auto Func = [&]() { mutex.lock(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
}

TEST(TestRadsan, UnlockingAMutexDiesWhenRealtime) {
  std::mutex mutex{};
  mutex.lock();
  auto Func = [&]() { mutex.unlock(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
}

#if RADSAN_TEST_SHARED_MUTEX

TEST(TestRadsan, LockingASharedMutexDiesWhenRealtime) {
  std::shared_mutex mutex{};
  auto Func = [&]() { mutex.lock(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
}

TEST(TestRadsan, UnlockingASharedMutexDiesWhenRealtime) {
  std::shared_mutex mutex{};
  mutex.lock();
  auto Func = [&]() { mutex.unlock(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
}

TEST(TestRadsan, SharedLockingASharedMutexDiesWhenRealtime) {
  std::shared_mutex mutex{};
  auto Func = [&]() { mutex.lock_shared(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
}

TEST(TestRadsan, SharedUnlockingASharedMutexDiesWhenRealtime) {
  std::shared_mutex mutex{};
  mutex.lock_shared();
  auto Func = [&]() { mutex.unlock_shared(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
}

#endif // RADSAN_TEST_SHARED_MUTEX

TEST(TestRadsan, launchingAThreadDiesWhenRealtime) {
  auto func = [&]() {
    std::thread Thread{[]() {}};
    Thread.join();
  };
  expectRealtimeDeath(func);
  expectNonrealtimeSurvival(func);
}

namespace {
void InvokeStdFunction(std::function<void()> &&function) { function(); }
} // namespace

TEST(TestRadsan, CopyingALambdaWithLargeCaptureDiesWhenRealtime) {
  std::array<float, 16> lots_of_data{};
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

TEST(TestRadsan, AccessingALargeAtomicVariableDiesWhenRealtime) {
  std::atomic<float> small_atomic{0.0f};
  ASSERT_TRUE(small_atomic.is_lock_free());
  RealtimeInvoke([&small_atomic]() { float x = small_atomic.load(); });

  std::atomic<std::array<float, 2048>> large_atomic{};
  ASSERT_FALSE(large_atomic.is_lock_free());
  auto Func = [&]() { auto x = large_atomic.load(); };
  ExpectRealtimeDeath(Func);
  ExpectNonRealtimeSurvival(Func);
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

TEST(TestRadsan, DoesNotDieIfTurnedOff) {
  std::mutex mutex{};
  auto RealtimeUnsafeFunc = [&]() {
    __radsan_off();
    mutex.lock();
    mutex.unlock();
    __radsan_on();
  };
  RealtimeInvoke(realtime_unsafe_func);
}
