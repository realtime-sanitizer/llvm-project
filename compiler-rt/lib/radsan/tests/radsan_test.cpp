#include "gtest/gtest.h"

auto vec = std::vector<float> (300);

[[clang::realtime]] int unsafeRealtimeFunction() 
{
  for (auto i = 0; i < 10; ++i)
    vec.push_back(static_cast<float> (i));
  return vec.size();
}

TEST (RealtimeSanitizer, newCausesDeath)
{
    EXPECT_DEATH (unsafeRealtimeFunction(), "radsan");
}