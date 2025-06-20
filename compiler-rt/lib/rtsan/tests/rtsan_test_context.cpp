//===--- rtsan_test_context.cpp - Realtime Sanitizer ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "rtsan_test_utilities.h"

#include "rtsan/rtsan.h"
#include "rtsan/rtsan_context.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace __rtsan;
using namespace ::testing;

class TestRtsanContext : public Test {
protected:
  void SetUp() override { __rtsan_ensure_initialized(); }
};

TEST_F(TestRtsanContext, IsNotRealtimeAfterDefaultConstruction) {
  Context context{};
  EXPECT_THAT(context.InRealtimeContext(), Eq(false));
}

TEST_F(TestRtsanContext, IsRealtimeAfterRealtimePush) {
  Context context{};
  context.RealtimePush();
  EXPECT_THAT(context.InRealtimeContext(), Eq(true));
}

TEST_F(TestRtsanContext, IsNotRealtimeAfterRealtimePushAndPop) {
  Context context{};
  context.RealtimePush();
  ASSERT_THAT(context.InRealtimeContext(), Eq(true));
  context.RealtimePop();
  EXPECT_THAT(context.InRealtimeContext(), Eq(false));
}

TEST_F(TestRtsanContext, RealtimeContextStateIsStatefullyTracked) {
  Context context{};
  auto const ExpectRealtime = [&context](bool is_rt) {
    EXPECT_THAT(context.InRealtimeContext(), Eq(is_rt));
  };
  ExpectRealtime(false);
  context.RealtimePush(); // depth 1
  ExpectRealtime(true);
  context.RealtimePush(); // depth 2
  ExpectRealtime(true);
  context.RealtimePop(); // depth 1
  ExpectRealtime(true);
  context.RealtimePush(); // depth 2
  ExpectRealtime(true);
  context.RealtimePop(); // depth 1
  ExpectRealtime(true);
  context.RealtimePop(); // depth 0
  ExpectRealtime(false);
  context.RealtimePush(); // depth 1
  ExpectRealtime(true);
}

TEST_F(TestRtsanContext, IsNotBypassedAfterDefaultConstruction) {
  Context context{};
  EXPECT_THAT(context.IsBypassed(), Eq(false));
}

TEST_F(TestRtsanContext, IsBypassedAfterBypassPush) {
  Context context{};
  context.BypassPush();
  EXPECT_THAT(context.IsBypassed(), Eq(true));
}

TEST_F(TestRtsanContext, BypassedStateIsStatefullyTracked) {
  Context context{};
  auto const ExpectBypassed = [&context](bool is_bypassed) {
    EXPECT_THAT(context.IsBypassed(), Eq(is_bypassed));
  };
  ExpectBypassed(false);
  context.BypassPush(); // depth 1
  ExpectBypassed(true);
  context.BypassPush(); // depth 2
  ExpectBypassed(true);
  context.BypassPop(); // depth 1
  ExpectBypassed(true);
  context.BypassPush(); // depth 2
  ExpectBypassed(true);
  context.BypassPop(); // depth 1
  ExpectBypassed(true);
  context.BypassPop(); // depth 0
  ExpectBypassed(false);
  context.BypassPush(); // depth 1
  ExpectBypassed(true);
}

TEST_F(TestRtsanContext, IsProbablyThreadSafe) {
  std::atomic<int> num_threads_started{0};
  std::atomic<bool> all_threads_wait{true};
  std::atomic<bool> all_threads_continue{true};
  Context context{};

  auto const expect_context_state =
      [&context](bool expected_in_realtime_context, bool expected_is_bypassed) {
        EXPECT_THAT(context.InRealtimeContext(),
                    Eq(expected_in_realtime_context));
        EXPECT_THAT(context.IsBypassed(), Eq(expected_is_bypassed));
      };

  auto const test_thread_work = [&]() {
    num_threads_started.fetch_add(1);
    while (all_threads_wait.load())
      std::this_thread::yield();

    while (all_threads_continue.load()) {
      context.RealtimePush();
      expect_context_state(true, false);
      context.RealtimePush();
      expect_context_state(true, false);

      context.BypassPush();
      expect_context_state(true, true);
      context.BypassPop();
      expect_context_state(true, false);

      context.RealtimePop();
      expect_context_state(true, false);
      context.RealtimePop();
      expect_context_state(false, false);
    }
  };

  auto const time_now = []() { return std::chrono::steady_clock::now(); };

  int const num_threads = 32;
  std::vector<std::thread> test_threads{};
  auto const start_time = std::chrono::steady_clock::now();
  for (int n = 0; n < num_threads; ++n)
    test_threads.push_back(std::thread(test_thread_work));

  std::chrono::duration timeout = std::chrono::milliseconds(100);
  while (num_threads_started.load() != num_threads) {
    if ((time_now() - start_time) > timeout) {
        FAIL();
    }
    std::this_thread::yield();
  }

  all_threads_wait.store(false);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  all_threads_continue.store(false);

  for (auto &test_thread : test_threads)
    test_thread.join();
}
