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

TEST(TestRadsanContext, CanCreateContext) { __radsan::Context Context{}; }

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieBeforeRealtimePush) {
  __radsan::Context Context{};
  Context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieAfterPushAndPop) {
  __radsan::Context Context{};
  Context.RealtimePush();
  Context.RealtimePop();
  Context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, ExpectNotRealtimeDiesAfterRealtimePush) {
  __radsan::Context Context{};

  Context.RealtimePush();
  EXPECT_DEATH(Context.ExpectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext,
     ExpectNotRealtimeDiesAfterRealtimeAfterMorePushesThanPops) {
  __radsan::Context Context{};

  Context.RealtimePush();
  Context.RealtimePush();
  Context.RealtimePush();
  Context.RealtimePop();
  Context.RealtimePop();
  EXPECT_DEATH(Context.ExpectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieAfterBypassPush) {
  __radsan::Context Context{};

  Context.RealtimePush();
  Context.BypassPush();
  Context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext,
     ExpectNotRealtimeDoesNotDieIfBypassDepthIsGreaterThanZero) {
  __radsan::Context Context{};

  Context.RealtimePush();
  Context.BypassPush();
  Context.BypassPush();
  Context.BypassPush();
  Context.BypassPop();
  Context.BypassPop();
  Context.ExpectNotRealtime("do_some_stuff");
  Context.BypassPop();
  EXPECT_DEATH(Context.ExpectNotRealtime("do_some_stuff"), "");
}
