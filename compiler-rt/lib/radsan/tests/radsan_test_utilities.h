/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#pragma once

#include "gmock/gmock.h"
#include <string>

namespace radsan_testing {

template <typename Function>
[[clang::realtime]] void realtimeInvoke(Function &&func) {
  std::forward<Function>(func)();
}

template <typename Function>
void expectRealtimeDeath(Function &&func,
                         const char *intercepted_method_name = nullptr) {

  using namespace testing;

  auto expected_error_substr = [&]() -> std::string {
    return intercepted_method_name != nullptr
               ? "Real-time violation: intercepted call to real-time unsafe "
                 "function `" +
                     std::string(intercepted_method_name) + "`"
               : "";
  };

  EXPECT_EXIT(realtimeInvoke(std::forward<Function>(func)),
              ExitedWithCode(EXIT_FAILURE), expected_error_substr());
}

template <typename Function> void expectNonrealtimeSurvival(Function &&func) {
  std::forward<Function>(func)();
}

} // namespace radsan_testing
