//===--- rtsan_test_expected.cpp - Realtime Sanitizer --*- C++ -*----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include <gmock/gmock.h>

#include "rtsan/rtsan_expected.h"

using namespace __rtsan;
using namespace ::testing;

namespace {
enum class Error { One, Two, Three };
}

TEST(TestRtsanExpected, hasDefaultValueAfterDefaultConstruction) {
  const Expected<int, Error> x{};
  ASSERT_THAT(x.HasValue(), Eq(true));
  EXPECT_THAT(x.Value(), Eq(int{}));
}

TEST(TestRtsanExpected, hasConstructionArgValueAfterValueConstruction) {
  const Expected<int, Error> x{42};
  ASSERT_THAT(x.HasValue(), Eq(true));
  EXPECT_THAT(x.Value(), Eq(42));
}

TEST(TestRtsanExpected, hasConstructionErrorAfterUnexpectedConstruction) {
  const Expected<int, Error> x{Unexpected<Error>{Error::Two}};
  ASSERT_THAT(x.HasValue(), Eq(false));
  EXPECT_THAT(x.Error(), Eq(Error::Two));
}

TEST(TestRtsanExpected, canBeImplicitlyConstructedFromUnexpected) {
  const Expected<int, Error> x = Unexpected<Error>{Error::Three};
  ASSERT_THAT(x.HasValue(), Eq(false));
  EXPECT_THAT(x.Error(), Eq(Error::Three));
}

TEST(TestRtsanExpected, canBeImplicitlyConstructedFromValue) {
  const Expected<int, Error> x = 42;
  ASSERT_THAT(x.HasValue(), Eq(true));
  EXPECT_THAT(x.Value(), Eq(42));
}

TEST(TestRtsanUnexpected, errorTypeCanBeDeducedFromConstructionArgument) {
  const auto e = Unexpected(Error::Three);
  EXPECT_THAT(e.Error(), Eq(Error::Three));
}

TEST(TestRtsanExpected, hasValueAfterDefaultConstructionWithVoidValueType) {
  const Expected<void, Error> x{};
  ASSERT_THAT(x.HasValue(), Eq(true));
}

TEST(TestRtsanExpected, hasErrorAfterUnexpectedConstructionWithVoidValueType) {
  const Expected<void, Error> x{Unexpected(Error::One)};
  ASSERT_THAT(x.HasValue(), Eq(false));
  EXPECT_THAT(x.Error(), Eq(Error::One));
}
