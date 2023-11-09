#pragma once

#include "gmock/gmock.h"

#include <optional>

namespace radsan_testing {

template <typename Function>
[[clang::realtime]] void realtimeInvoke(Function &&func) {
  std::forward<Function>(func)();
}

template <typename Function>
void expectRealtimeDeath(
    Function &&func, std::optional<std::string> intercepted_method_name = {}) {

  using namespace testing;

  auto expected_error_substr = [&]() -> std::string {
    return intercepted_method_name.has_value()
               ? "Intercepted call to realtime-unsafe function `" +
                     intercepted_method_name.value() + "`"
               : "";
  };

  EXPECT_EXIT(realtimeInvoke(std::forward<Function>(func)),
              ExitedWithCode(EXIT_FAILURE), expected_error_substr());
}

template <typename Function> void expectNonrealtimeSurvival(Function &&func) {
  std::forward<Function>(func)();
}

} // namespace radsan_testing
