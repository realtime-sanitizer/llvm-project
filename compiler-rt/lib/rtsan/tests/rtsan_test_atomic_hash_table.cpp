//===--- rtsan_test_atomic_hash_table.cpp - Realtime Sanitizer --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include <gmock/gmock.h>

#include "rtsan/rtsan_atomic_hash_table.h"

using namespace __rtsan;
using namespace ::testing;

namespace {
using TestTableTy = AtomicHashTable<int, float, 999, 1000>;

class MoveOnly {
public:
  MoveOnly(int val) : val_(val) {}
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly &operator=(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&) = default;
  MoveOnly &operator=(MoveOnly &&) = default;
  int value() const { return val_; }

private:
  int val_{};
};
} // namespace

TEST(TestRtsanAtomicHashTable, sizeIsZeroAfterDefaultConstruction) {
  const TestTableTy table{1};
  EXPECT_THAT(table.ApproxSize(), Eq(0));
}

TEST(TestRtsanAtomicHashTable, capacityIsReportedAsSameAsConstructionArg) {
  const TestTableTy table{100};
  EXPECT_THAT(table.Capacity(), Eq(100));
}

TEST(TestRtsanAtomicHashTable, sizeIsIncreasedWithInsertions) {
  TestTableTy table{100};
  EXPECT_THAT(table.ApproxSize(), Eq(0));
  for (int n = 10; n < 25; ++n) {
    ASSERT_THAT(table.Insert(n, 4.0f), Eq(HashTableInsertResult::OK));
    EXPECT_THAT(table.ApproxSize(), Eq(n - 10 + 1));
  }
}

TEST(TestRtsanAtomicHashTable, searchForNonexistentValueReturnsEmptyResult) {
  TestTableTy table{10};
  ASSERT_THAT(table.Insert(7, 7.0f), Eq(HashTableInsertResult::OK));

  auto const test_search = [](auto &t) {
    EXPECT_THAT(t.Search(6).HasValue(), Eq(false));
  };

  auto &mutable_table = table;
  const auto &const_table = table;
  test_search(const_table);
  test_search(mutable_table);
}

TEST(TestRtsanAtomicHashTable, canInsertAndRetrieveValuesBothConstAndMutable) {
  TestTableTy table{100};
  ASSERT_THAT(table.Insert(3, 3.0f), Eq(HashTableInsertResult::OK));
  ASSERT_THAT(table.Insert(5, 5.0f), Eq(HashTableInsertResult::OK));

  auto const test_search_const = [](TestTableTy const &t) {
    auto const three = t.Search(3);
    auto const five = t.Search(5);
    ASSERT_THAT(three.HasValue(), Eq(true));
    ASSERT_THAT(five.HasValue(), Eq(true));
    EXPECT_THAT(three.Value(), Eq(3.0f));
    EXPECT_THAT(five.Value(), Eq(5.0f));
  };

  auto const test_search_mutation = [](TestTableTy &t) {
    auto number = t.Search(3);
    ASSERT_THAT(number.HasValue(), Eq(true));
    EXPECT_THAT(number.Value(), Eq(3.0f));
    number.Value() = 3000.0f;
    EXPECT_THAT(t.Search(3).Value(), Eq(3000.0f));
  };

  test_search_const(table);
  test_search_mutation(table);
}

TEST(TestRtsanAtomicHashTable, canInsertTypesConvertibleToValueType) {
  TestTableTy table{5};
  ASSERT_THAT(table.Insert(12, 10), Eq(HashTableInsertResult::OK));
  const auto number = table.Search(12);
  ASSERT_THAT(number.HasValue(), Eq(true));
  EXPECT_THAT(number.Value(), Eq(10.0f));
}

TEST(TestRtsanAtomicHashTable, canInsertRValueTypes) {
  using TableTy = AtomicHashTable<int, MoveOnly, 999, 1000>;
  TableTy table{5};
  MoveOnly move_me{10};
  ASSERT_THAT(table.Insert(12, std::move(move_me)),
              Eq(HashTableInsertResult::OK));
  const auto result = table.Search(12);
  ASSERT_THAT(result.HasValue(), Eq(true));
  EXPECT_THAT(result.Value().value(), Eq(10));
}

TEST(TestRtsanAtomicHashTable, canInsertImmovableValueTypes) {
  using TableTy = AtomicHashTable<int, int, 999, 1000>;
  TableTy table{5};
  const int value{11};
  ASSERT_THAT(table.Insert(12, value), Eq(HashTableInsertResult::OK));
  const auto result = table.Search(12);
  ASSERT_THAT(result.HasValue(), Eq(true));
  EXPECT_THAT(result.Value(), Eq(value));
}

TEST(TestRtsanAtomicHashTable, canRepeatedlyInsertAtTheSameKey) {
  TestTableTy table{5};
  constexpr size_t num_inserts = 10u;
  for (size_t n = 0u; n <= num_inserts; ++n) {
    ASSERT_THAT(table.Insert(13, 13.0f + static_cast<float>(n)),
                Eq(HashTableInsertResult::OK));
  }
  const auto result = table.Search(13);
  ASSERT_THAT(result.HasValue(), Eq(true));
  EXPECT_THAT(result.Value(), Eq(13.0f + static_cast<float>(num_inserts)));
}

TEST(TestRtsanAtomicHashTable, canRemoveEntries) {
  TestTableTy table{5};
  ASSERT_THAT(table.Insert(1, 1.0f), Eq(HashTableInsertResult::OK));
  ASSERT_THAT(table.Insert(2, 2.0f), Eq(HashTableInsertResult::OK));
  ASSERT_THAT(table.Insert(3, 3.0f), Eq(HashTableInsertResult::OK));

  // Collision with key 3
  ASSERT_THAT(table.Insert(3 + 5, 8.0f), Eq(HashTableInsertResult::OK));

  const auto test_remove = [&table](int key) {
    ASSERT_THAT(table.Search(key).HasValue(), Eq(true));
    EXPECT_THAT(table.Remove(key), Eq(HashTableRemoveResult::Removed));
    ASSERT_THAT(table.Search(key).HasValue(), Eq(false));
    EXPECT_THAT(table.Remove(key), Eq(HashTableRemoveResult::NotFound));
  };

  EXPECT_THAT(table.ApproxSize(), Eq(4ul));
  test_remove(2);
  EXPECT_THAT(table.ApproxSize(), Eq(3ul));
  test_remove(8);
  EXPECT_THAT(table.ApproxSize(), Eq(2ul));
  test_remove(3);
  EXPECT_THAT(table.ApproxSize(), Eq(1ul));
}

TEST(TestRtsanAtomicHashTable, canInsertAndRetrieveKeysWithHashCollision) {
  using HashFn = DefaultHashFn<int>;
  using SlotFn = DefaultSlotFn<int, HashFn>;
  using TableTy = AtomicHashTable<int, float, 999, 1000, SlotFn>;
  const size_t capacity = 10ul;
  const SlotFn hash_slot{};
  std::vector<int> colliding_keys = {7, 17, 27, 37, 47, 57, 67, 77};
  for (auto key : colliding_keys) {
    ASSERT_THAT(hash_slot(key, capacity),
                Eq(hash_slot(colliding_keys.front(), capacity)));
  }

  TableTy table{10};
  for (auto key : colliding_keys) {
    ASSERT_THAT(table.Insert(key, static_cast<float>(key)),
                Eq(HashTableInsertResult::OK));
  }

  auto const check_keys = [&colliding_keys](auto &t) {
    for (auto key : colliding_keys) {
      const auto result = t.Search(key);
      ASSERT_THAT(result.HasValue(), Eq(true));
      EXPECT_THAT(result.Value(), Eq(static_cast<float>(key)));
    }
  };

  auto &mutable_table = table;
  const auto &const_table = table;
  check_keys(mutable_table);
  check_keys(const_table);
}

TEST(TestRtsanAtomicHashTable, insertReportsOverflowOnOverflow) {
  TestTableTy table{5};
  for (size_t n = 0u; n < table.Capacity(); ++n) {
    ASSERT_THAT(table.Insert(static_cast<int>(n), static_cast<float>(n)),
                Eq(HashTableInsertResult::OK));
  }
  EXPECT_THAT(table.Insert(100, 10.0f), Eq(HashTableInsertResult::Overflow));
  EXPECT_THAT(table.Insert(101, 10.0f), Eq(HashTableInsertResult::Overflow));
}

TEST(TestRtsanAtomicHashTable,
     DISABLED_handlesManyInsertionsAndRemovalsIncludingCollidingHashes) {
  FAIL() << "TODO";
}

TEST(TestRtsanAtomicHashTable, DISABLED_multiThreadedStressTest) {
  FAIL() << "TODO";
}
