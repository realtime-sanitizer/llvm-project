//===--- radsan_test_context.cpp - Realtime Sanitizer --------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "radsan_test_utilities.h"

#include "radsan_context.h"

TEST(TestRadsanContext, CanCreateContext) { __radsan::Context context{}; }

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieBeforeRealtimePush) {
  __radsan::Context context{};
  context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieAfterPushAndPop) {
  __radsan::Context context{};
  context.RealtimePush();
  context.RealtimePop();
  context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, ExpectNotRealtimeDiesAfterRealtimePush) {
  __radsan::Context context{};

  context.RealtimePush();
  EXPECT_DEATH(context.ExpectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext,
     ExpectNotRealtimeDiesAfterRealtimeAfterMorePushesThanPops) {
  __radsan::Context context{};

  context.RealtimePush();
  context.RealtimePush();
  context.RealtimePush();
  context.RealtimePop();
  context.RealtimePop();
  EXPECT_DEATH(context.ExpectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieAfterBypassPush) {
  __radsan::Context context{};

  context.RealtimePush();
  context.BypassPush();
  context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext,
     ExpectNotRealtimeDoesNotDieIfBypassDepthIsGreaterThanZero) {
  __radsan::Context context{};

  context.RealtimePush();
  context.BypassPush();
  context.BypassPush();
  context.BypassPush();
  context.BypassPop();
  context.BypassPop();
  context.ExpectNotRealtime("do_some_stuff");
  context.BypassPop();
  EXPECT_DEATH(context.ExpectNotRealtime("do_some_stuff"), "");
}
