//===--- radsan_test_utilities.h - Realtime Sanitizer --------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "radsan.h"
#include "gmock/gmock.h"
#include <string>

namespace radsan_testing {

template <typename Function> void RealtimeInvoke(Function &&Func) {
  __radsan_realtime_enter();
  std::forward<Function>(Func)();
  __radsan_realtime_exit();
}

template <typename Function>
void expectRealtimeDeath(Function &&Func,
                         const char *intercepted_method_name = nullptr) {

  using namespace testing;

  auto GetExpectedErrorSubstring = [&]() -> std::string {
    return intercepted_method_name != nullptr
               ? "Real-time violation: intercepted call to real-time unsafe "
                 "function `" +
                     std::string(intercepted_method_name) + "`"
               : "";
  };

  EXPECT_EXIT(RealtimeInvoke(std::forward<Function>(Func)),
              ExitedWithCode(EXIT_FAILURE), GetExpectedErrorSubstring());
}

template <typename Function> void expectNonrealtimeSurvival(Function &&Func) {
  std::forward<Function>(Func)();
}

} // namespace radsan_testing
