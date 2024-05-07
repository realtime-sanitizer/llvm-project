/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan_test_utilities.h"

#include "radsan_context.h"

TEST(TestRadsanContext, canCreateContext) { auto context = radsan::Context{}; }

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieBeforeRealtimePush) {
  auto context = radsan::Context{};
  context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieAfterPushAndPop) {
  auto context = radsan::Context{};
  context.RealtimePush();
  context.RealtimePop();
  context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext, ExpectNotRealtimeDiesAfterRealtimePush) {
  auto context = radsan::Context{};

  context.RealtimePush();
  EXPECT_DEATH(context.ExpectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext,
     ExpectNotRealtimeDiesAfterRealtimeAfterMorePushesThanPops) {
  auto context = radsan::Context{};

  context.RealtimePush();
  context.RealtimePush();
  context.RealtimePush();
  context.RealtimePop();
  context.RealtimePop();
  EXPECT_DEATH(context.ExpectNotRealtime("do_some_stuff"), "");
}

TEST(TestRadsanContext, ExpectNotRealtimeDoesNotDieAfterBypassPush) {
  auto context = radsan::Context{};

  context.RealtimePush();
  context.BypassPush();
  context.ExpectNotRealtime("do_some_stuff");
}

TEST(TestRadsanContext,
     ExpectNotRealtimeDoesNotDieIfBypassDepthIsGreaterThanZero) {
  auto context = radsan::Context{};

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
