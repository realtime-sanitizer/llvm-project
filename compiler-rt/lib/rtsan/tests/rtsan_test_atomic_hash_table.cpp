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
using InsertError = TestTableTy::InsertError;
using RemoveError = TestTableTy::RemoveError;

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

template <typename T, size_t Size> class HistoryBuffer {
public:
  HistoryBuffer(T init = T{}) {
    std::fill(std::begin(data_), std::end(data_), init);
  }

  template <typename U> void Push(U &&item) {
    data_[cursor_] = std::forward<U>(item);
    CircularIncrement(cursor_);
  }

  T LookBack(size_t num_items) {
    return data_[(cursor_ + Size - num_items) % Size];
  }

private:
  static void CircularIncrement(size_t &index) { index = (index + 1) % Size; }

  std::array<T, Size> data_;
  size_t cursor_{0u};
};
} // namespace

namespace __rtsan {
std::ostream &operator<<(std::ostream &os, InsertError const &error) {
  switch (error) {
  case InsertError::AlreadyExists:
    return os << "AlreadyExists";
  case InsertError::Overflow:
    return os << "Overflow";
  }

  return os;
}
std::ostream &operator<<(std::ostream &os, RemoveError const &error) {
  switch (error) {
  case RemoveError::NotFound:
    return os << "NotFound";
  }

  return os;
}
} // namespace __rtsan

TEST(TestRtsanAtomicHashTable, approxSizeIsZeroAfterDefaultConstruction) {
  const TestTableTy table{1};
  EXPECT_THAT(table.ApproxSize(), Eq(0));
}

TEST(TestRtsanAtomicHashTable, capacityIsSameAsConstructionArg) {
  const TestTableTy table{100};
  EXPECT_THAT(table.Capacity(), Eq(100));
}

TEST(TestRtsanAtomicHashTable,
     approxSizeIsIncreasedWithInsertionsAndDecreasedWithRemovals) {
  TestTableTy table{100};
  EXPECT_THAT(table.ApproxSize(), Eq(0));

  const int start = 10;
  const int finish = 25;
  for (int n = start; n < finish; ++n) {
    ASSERT_THAT(table.Insert(n, 1.0f).HasValue(), Eq(true));
    EXPECT_THAT(table.ApproxSize(), Eq(n - start + 1));
  }
  for (int n = start; n < finish; ++n) {
    ASSERT_THAT(table.Remove(n).HasValue(), Eq(true));
    EXPECT_THAT(table.ApproxSize(), Eq(finish - n - 1));
  }
}

TEST(TestRtsanAtomicHashTable, searchForNonexistentValueReturnsEmptyResult) {
  TestTableTy table{10};
  ASSERT_THAT(table.Insert(7, 7.0f).HasValue(), Eq(true));

  const auto test_search = [](auto &t) {
    EXPECT_THAT(t.Search(6).HasValue(), Eq(false));
  };

  TestTableTy &mutable_table = table;
  const TestTableTy &const_table = table;
  test_search(const_table);
  test_search(mutable_table);
}

TEST(TestRtsanAtomicHashTable, canInsertAndRetrieveValuesBothConstAndMutable) {
  TestTableTy table{100};
  ASSERT_THAT(table.Insert(3, 3.0f).HasValue(), Eq(true));
  ASSERT_THAT(table.Insert(5, 5.0f).HasValue(), Eq(true));

  const auto test_search_const = [](TestTableTy const &t) {
    const auto three = t.Search(3);
    const auto five = t.Search(5);
    ASSERT_THAT(three.HasValue(), Eq(true));
    ASSERT_THAT(five.HasValue(), Eq(true));
    EXPECT_THAT(*three.Value(), Eq(3.0f));
    EXPECT_THAT(*five.Value(), Eq(5.0f));
  };

  const auto test_search_mutation = [](TestTableTy &t) {
    auto number = t.Search(3);
    ASSERT_THAT(number.HasValue(), Eq(true));
    EXPECT_THAT(*number.Value(), Eq(3.0f));
    *number.Value() = 3000.0f;
    EXPECT_THAT(*(t.Search(3).Value()), Eq(3000.0f));
  };

  test_search_const(table);
  test_search_mutation(table);
}

TEST(TestRtsanAtomicHashTable, canInsertTypesConvertibleToValueType) {
  TestTableTy table{5};
  ASSERT_THAT(table.Insert(12, 10).HasValue(), Eq(true));
  const auto number = table.Search(12);
  ASSERT_THAT(number.HasValue(), Eq(true));
  EXPECT_THAT(*number.Value(), Eq(10.0f));
}

TEST(TestRtsanAtomicHashTable, canInsertRValueTypes) {
  using TableTy = AtomicHashTable<int, MoveOnly, 999, 1000>;
  TableTy table{5};
  MoveOnly move_me{10};
  ASSERT_THAT(table.Insert(12, std::move(move_me)).HasValue(), Eq(true));
  const auto result = table.Search(12);
  ASSERT_THAT(result.HasValue(), Eq(true));
  EXPECT_THAT(result.Value()->value(), Eq(10));
}

TEST(TestRtsanAtomicHashTable, canInsertImmovableValueTypes) {
  using TableTy = AtomicHashTable<int, int, 999, 1000>;
  TableTy table{5};
  const int value{11};
  ASSERT_THAT(table.Insert(12, value).HasValue(), Eq(true));
  const auto result = table.Search(12);
  ASSERT_THAT(result.HasValue(), Eq(true));
  EXPECT_THAT(*result.Value(), Eq(value));
}

TEST(TestRtsanAtomicHashTable, insertAtAnExistingKeyFails) {
  TestTableTy table{5};
  ASSERT_THAT(table.Insert(13, 13.0f).HasValue(), Eq(true));
  const auto result = table.Search(13);
  ASSERT_THAT(result.HasValue(), Eq(true));
  ASSERT_THAT(*result.Value(), Eq(13.0f));

  const auto insert_result = table.Insert(13, 14.0f);
  ASSERT_THAT(insert_result.HasValue(), Eq(false));
  EXPECT_THAT(insert_result.Error(), Eq(InsertError::AlreadyExists));
}

TEST(TestRtsanAtomicHashTable, canRemoveEntries) {
  const size_t capacity = 5;
  TestTableTy table{capacity};
  ASSERT_THAT(table.Insert(1, 1.0f).HasValue(), Eq(true));
  ASSERT_THAT(table.Insert(2, 2.0f).HasValue(), Eq(true));
  ASSERT_THAT(table.Insert(3, 3.0f).HasValue(), Eq(true));

  // Collision with key 3
  ASSERT_THAT(table.Insert(3 + capacity, 3.0f + capacity).HasValue(), Eq(true));

  const auto test_remove = [&table](int key) {
    ASSERT_THAT(table.Search(key).HasValue(), Eq(true));
    EXPECT_THAT(table.Remove(key).HasValue(), Eq(true));
    ASSERT_THAT(table.Search(key).HasValue(), Eq(false));
    const auto remove_result = table.Remove(key);
    ASSERT_THAT(remove_result.HasValue(), Eq(false));
    EXPECT_THAT(remove_result.Error(), Eq(RemoveError::NotFound));
  };

  EXPECT_THAT(table.ApproxSize(), Eq(4ul));
  test_remove(2);
  EXPECT_THAT(table.ApproxSize(), Eq(3ul));
  test_remove(3 + capacity);
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
  for (int key : colliding_keys) {
    ASSERT_THAT(hash_slot(key, capacity),
                Eq(hash_slot(colliding_keys.front(), capacity)));
  }

  TableTy table{capacity};
  for (int key : colliding_keys) {
    ASSERT_THAT(table.Insert(key, static_cast<float>(key)).HasValue(),
                Eq(true));
  }

  const auto check_keys = [&colliding_keys](auto &t) {
    for (int key : colliding_keys) {
      const auto result = t.Search(key);
      ASSERT_THAT(result.HasValue(), Eq(true));
      EXPECT_THAT(*result.Value(), Eq(static_cast<float>(key)));
    }
  };

  TableTy &mutable_table = table;
  const TableTy &const_table = table;
  check_keys(mutable_table);
  check_keys(const_table);
}

TEST(TestRtsanAtomicHashTable, insertReportsOverflowOnOverflow) {
  TestTableTy table{5};
  for (size_t n = 0u; n < table.Capacity(); ++n) {
    ASSERT_THAT(
        table.Insert(static_cast<int>(n), static_cast<float>(n)).HasValue(),
        Eq(true));
  }
  const auto expect_overflow = [&table](int key, float value) {
    const auto insert_result = table.Insert(key, value);
    ASSERT_THAT(insert_result.HasValue(), Eq(false));
    EXPECT_THAT(insert_result.Error(), Eq(InsertError::Overflow));
  };

  expect_overflow(100, 10.0f);
  expect_overflow(101, 10.1f);
}

TEST(TestRtsanAtomicHashTable, canReclaimTombstonesCorrectly) {
  TestTableTy table{3};
  ASSERT_THAT(table.Insert(0, 0.0f).HasValue(), Eq(true));
  ASSERT_THAT(table.Insert(1, 1.0f).HasValue(), Eq(true));
  ASSERT_THAT(table.Insert(2, 2.0f).HasValue(), Eq(true));
  ASSERT_THAT(table.Remove(0).HasValue(), Eq(true));
  ASSERT_THAT(table.Remove(1).HasValue(), Eq(true));
  ASSERT_THAT(table.Remove(2).HasValue(), Eq(true));
  EXPECT_THAT(table.Insert(2, 2000.0f).HasValue(), Eq(true));
}

TEST(TestRtsanAtomicHashTable,
     handlesManyInsertionsAndRemovalsIncludingCollidingHashes) {
  using SlotFn = DefaultSlotFn<int, DefaultHashFn<int>>;
  using TableTy = AtomicHashTable<int, int, 999, 1000, SlotFn>;
  constexpr size_t capacity{10u};
  TableTy table{capacity};
  HistoryBuffer<int, capacity> inserted_key_history{};
  const std::vector<int> keys = {4,  14, 24,  34,  44,  54,  64,  74,
                                 84, 94, 104, 114, 124, 134, 144, 154};
  const auto make_value = [](int key) { return 100 * key; };
  for (size_t n = 0u; n < capacity; ++n) {
    ASSERT_THAT(table.Insert(keys[n], make_value(keys[n])).HasValue(),
                Eq(true));
    inserted_key_history.Push(keys[n]);
  }

  const auto assert_all_inserted_keys_findable = [&]() {
    for (auto n = 0u; n < capacity; ++n) {
      const int key = inserted_key_history.LookBack(capacity - n);
      ASSERT_THAT(table.Search(key).HasValue(), Eq(true));
    }
  };

  for (int repeat = 0; repeat < 1; ++repeat) {
    for (int key : keys) {
      assert_all_inserted_keys_findable();
      const int key_to_remove = inserted_key_history.LookBack(capacity);
      inserted_key_history.Push(key);
      ASSERT_THAT(table.Remove(key_to_remove).HasValue(), Eq(true));
      ASSERT_THAT(table.Insert(key, make_value(key)).HasValue(), Eq(true));
    }
  }
}

TEST(TestRtsanAtomicHashTable, DISABLED_multiThreadedStressTest) {
  FAIL() << "TODO";
}
