#include "radsan_test_utilities.h"

#include "radsan_context.h"
#include "radsan_user_interface.h"

namespace
{
class MockUserInterface : public radsan::IUserInterface {
public:
    MockUserInterface()
    {
        ON_CALL(*this, getAction).WillByDefault(testing::Return(
            radsan::OnErrorAction::ExitWithFailure));
    }

    MOCK_METHOD(radsan::OnErrorAction, getAction, (), (override));
};
}

TEST (TestRadsanContext, canCreateContext)
{
    auto context = radsan::Context{};
}

TEST (TestRadsanContext, expectNotRealtimeDoesNotDieBeforeRealtimePush)
{
    auto context = radsan::Context{};
    context.expectNotRealtime("do_some_stuff");
}

TEST (TestRadsanContext, expectNotRealtimeDoesNotDieAfterPushAndPop)
{
    auto context = radsan::Context{};
    context.realtimePush();
    context.realtimePop();
    context.expectNotRealtime("do_some_stuff");
}

TEST (TestRadsanContext, expectNotRealtimeDiesAfterRealtimePush)
{
    auto context = radsan::Context{};

    context.realtimePush();
    EXPECT_DEATH (context.expectNotRealtime("do_some_stuff"), "");
}

TEST (TestRadsanContext, expectNotRealtimeDiesAfterRealtimeAfterMorePushesThanPops)
{
    auto context = radsan::Context{};

    context.realtimePush();
    context.realtimePush();
    context.realtimePush();
    context.realtimePop();
    context.realtimePop();
    EXPECT_DEATH (context.expectNotRealtime("do_some_stuff"), "");
}

TEST (TestRadsanContext, expectNotRealtimeDoesNotDieAfterBypassPush)
{
    auto context = radsan::Context{};

    context.realtimePush();
    context.bypassPush();
    context.expectNotRealtime("do_some_stuff");
}

TEST (TestRadsanContext, expectNotRealtimeDoesNotDieIfBypassDepthIsGreaterThanZero)
{
    auto context = radsan::Context{};

    context.realtimePush();
    context.bypassPush();
    context.bypassPush();
    context.bypassPush();
    context.bypassPop();
    context.bypassPop();
    context.expectNotRealtime("do_some_stuff");
    context.bypassPop();
    EXPECT_DEATH (context.expectNotRealtime("do_some_stuff"), "");
}

TEST (TestRadsanContext, onlyDiesIfExitWithFailureReturnedFromUser)
{
    auto mock_user = std::make_unique<MockUserInterface>();
    auto & mock_user_ref = *mock_user;
    testing::Mock::AllowLeak(mock_user.get());

    auto context = radsan::Context{std::move(mock_user)};
    context.realtimePush();

    EXPECT_CALL(mock_user_ref, getAction).WillOnce(
        testing::Return (radsan::OnErrorAction::Continue));
    context.expectNotRealtime("do_some_stuff_expecting_continue");

    ON_CALL(mock_user_ref, getAction).WillByDefault(
        testing::Return (radsan::OnErrorAction::ExitWithFailure));
    EXPECT_DEATH(context.expectNotRealtime("do_some_stuff_expecting_exit"), "");
}
