/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#pragma once

#include "gmock/gmock.h"

#include <functional>
#include <string>

namespace radsan_testing {

namespace detail {
/*
    Should inject radsan_realtime_enter?

    [[clang::nonblocking]]                 YES
    [[clang::nonblocking(true)]]           YES
    [[clang::blocking(false)]]             YES

    [[clang::nonblocking(false)]]          NO
    [[clang::blocking]]                    NO
    [[clang::blocking(true)]]              NO
*/

template <typename Function>
void nonblockingInvoke(Function &&func) [[clang::nonblocking]] {
  func();
}

template <typename Function>
void nonblockingTrueInvoke(Function &&func) [[clang::nonblocking(true)]] {
  func();
}

template <typename Function>
void nonblockingFalseInvoke(Function &&func) [[clang::nonblocking(false)]] {
  func();
}

template <typename Function>
void blockingInvoke(Function &&func) [[clang::blocking]] {
  func();
}

template <typename Function>
void blockingTrueInvoke(Function &&func) [[clang::blocking(true)]] {
  func();
}

template <typename Function>
void blockingFalseInvoke(Function &&func) [[clang::blocking(false)]] {
  func();
}

template <typename Function>
void invokeWithAllNonBlockingAttributes(Function &&func) {
  nonblockingInvoke(func);
  nonblockingTrueInvoke(func);
  blockingFalseInvoke(func);
}

template <typename Function>
void invokeWithAllBlockingAttributes(Function &&func,
                                     std::function<void()> reset) {
  blockingInvoke(func);
  reset();

  blockingTrueInvoke(func);
  reset();

  nonblockingFalseInvoke(func);
  reset();

  func();
  reset();
}

template <typename Function> void invokeWithNoAttributes(Function &&func) {
  func();
}
} // namespace detail

template <typename Function>
void expectSurvivalInNonBlockingContext(Function &&func) {
  detail::invokeWithAllNonBlockingAttributes(std::forward<Function>(func));
}

template <typename Function>
void expectDeathInNonBlockingContext(
    Function &&func, const char *intercepted_method_name = nullptr) {

  using namespace testing;

  auto expected_error_substr = [&]() -> std::string {
    return intercepted_method_name != nullptr
               ? "Real-time violation: intercepted call to real-time unsafe "
                 "function `" +
                     std::string(intercepted_method_name) + "`"
               : "";
  };

  EXPECT_EXIT(detail::nonblockingInvoke(func), ExitedWithCode(EXIT_FAILURE),
              expected_error_substr());
  EXPECT_EXIT(detail::nonblockingTrueInvoke(func), ExitedWithCode(EXIT_FAILURE),
              expected_error_substr());
  EXPECT_EXIT(detail::blockingFalseInvoke(func), ExitedWithCode(EXIT_FAILURE),
              expected_error_substr());
}

template <typename Function>
void expectSurvivalInBlockableContext(
    Function &&func, std::function<void()> reset = []() {}) {
  detail::invokeWithAllBlockingAttributes(func, reset);
  detail::invokeWithNoAttributes(func);
}

} // namespace radsan_testing
