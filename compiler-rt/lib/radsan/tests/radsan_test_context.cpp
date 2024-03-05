/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan_test_utilities.h"

#include "radsan_context.h"

TEST(TestRadsanContext, canCreateContext) { auto context = radsan::Context{}; }

TEST(TestRadsanContext, expectNotRealtimeDoesNotDieBeforeRealtimePush) {
  auto context = radsan::Context{};
  context.expectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, expectNotRealtimeDoesNotDieAfterPushAndPop) {
  auto context = radsan::Context{};
  context.realtimePush();
  context.realtimePop();
  context.expectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, expectNotRealtimeDiesAfterRealtimePush) {
  auto context = radsan::Context{};

  context.realtimePush();
  EXPECT_DEATH(context.expectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext,
     expectNotRealtimeDiesAfterRealtimeAfterMorePushesThanPops) {
  auto context = radsan::Context{};

  context.realtimePush();
  context.realtimePush();
  context.realtimePush();
  context.realtimePop();
  context.realtimePop();
  EXPECT_DEATH(context.expectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext, expectNotRealtimeDoesNotDieAfterBypassPush) {
  auto context = radsan::Context{};

  context.realtimePush();
  context.bypassPush();
  context.expectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext,
     expectNotRealtimeDoesNotDieIfBypassDepthIsGreaterThanZero) {
  auto context = radsan::Context{};

  context.realtimePush();
  context.bypassPush();
  context.bypassPush();
  context.bypassPush();
  context.bypassPop();
  context.bypassPop();
  context.expectNotRealtime("do_some_stuff");
  context.bypassPop();
  EXPECT_DEATH(context.expectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext, onlyDiesIfExitWithFailureReturnedFromUser) {
}
