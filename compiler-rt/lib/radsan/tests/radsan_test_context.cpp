#include "radsan_test_utilities.h"

#include "radsan_context.h"

TEST (TestRadsanContext, canCreateContext)
{
    auto context = radsan::Context{};
}

TEST (TestRadsanContext, exitIfRealtimeDoesNotDieBeforeRealtimePush)
{
    auto context = radsan::Context{};
    context.exitIfRealtime("do_some_stuff");
}

TEST (TestRadsanContext, exitIfRealtimeDoesNotDieAfterPushAndPop)
{
    auto context = radsan::Context{};
    context.realtimePush();
    context.realtimePop();
    context.exitIfRealtime("do_some_stuff");
}

TEST (TestRadsanContext, exitIfRealtimeDiesAfterRealtimePush)
{
    auto context = radsan::Context{};

    context.realtimePush();
    EXPECT_DEATH (context.exitIfRealtime("do_some_stuff"), "");
}

TEST (TestRadsanContext, exitIfRealtimeDiesAfterRealtimeAfterMorePushesThanPops)
{
    auto context = radsan::Context{};

    context.realtimePush();
    context.realtimePush();
    context.realtimePush();
    context.realtimePop();
    context.realtimePop();
    EXPECT_DEATH (context.exitIfRealtime("do_some_stuff"), "");
}
