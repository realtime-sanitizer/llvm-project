//===--- rtsan_test_optional.cpp - Realtime Sanitizer --*- C++ -*----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include <gmock/gmock.h>

#include "rtsan/rtsan_optional.h"

using namespace ::testing;

TEST(TestRtsanOptional, hasNoValueAfterDefaultConstruction) {
  const __rtsan::Optional<int> x{};
  ASSERT_THAT(x.HasValue(), Eq(false));
}

TEST(TestRtsanOptional, hasConstructionArgValueAfterValueConstruction) {
  const __rtsan::Optional<int> x{42};
  ASSERT_THAT(x.HasValue(), Eq(true));
  EXPECT_THAT(x.Value(), Eq(42));
}

TEST(TestRtsanOptional, hasNoValueAfterNulloptConstruction) {
  const __rtsan::Optional<int> x{__rtsan::Nullopt{}};
  ASSERT_THAT(x.HasValue(), Eq(false));
}

TEST(TestRtsanOptional, canBeImplicitlyConstructedFromNullopt) {
  const __rtsan::Optional<int> x = __rtsan::Nullopt{};
  ASSERT_THAT(x.HasValue(), Eq(false));
}

TEST(TestRtsanOptional, canBeImplicitlyConstructedFromValue) {
  const __rtsan::Optional<int> x = 42;
  ASSERT_THAT(x.HasValue(), Eq(true));
  EXPECT_THAT(x.Value(), Eq(42));
}
