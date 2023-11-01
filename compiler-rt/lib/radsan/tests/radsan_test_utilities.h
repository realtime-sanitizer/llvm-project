#pragma once

#include "gtest/gtest.h"

namespace radsan_testing
{

template <typename Function>
[[clang::realtime]] void realtimeInvoke(Function &&func) {
  std::forward<Function>(func)();
}
template <typename Function> void expectRealtimeDeath(Function &&func) {
  EXPECT_DEATH(realtimeInvoke(std::forward<Function>(func)), "radsan");
}

template <typename Function> void expectNonrealtimeSurvival(Function &&func) {
  std::forward<Function>(func)();
}

}
