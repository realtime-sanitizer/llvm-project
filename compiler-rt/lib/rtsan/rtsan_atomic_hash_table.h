//===--- rtsan_atomic_hash_table.h - Realtime Sanitizer ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#pragma once

#include "sanitizer_common/sanitizer_atomic.h"
#include "sanitizer_common/sanitizer_vector.h"

// TODO is it OK to include these C++ headers for size_t and std::move?
#include <cstddef>
#include <utility>

namespace __rtsan {

// TODO actually implement conversion and move into own header
// (and add header to CMake proj!)
template <typename T> struct AtomicKey {
  // TODO move into generic failure struct
  static_assert(
      sizeof(T) <= sizeof(__sanitizer::atomic_uint64_t),
      "Key type must be a type that is supported as atomic in compiler-rt");
  using Type = __sanitizer::atomic_uint64_t;
};

template <> struct AtomicKey<int> {
  using Type = __sanitizer::atomic_sint32_t;
};

enum class HashTableInsertResult { OK, Overflow };
enum class HashTableRemoveResult { Removed, NotFound };

template <typename ValueTy> class HashTableSearchResult {
public:
  static HashTableSearchResult Found(ValueTy *value_addr) {
    return HashTableSearchResult(value_addr);
  }
  static HashTableSearchResult NotFound() {
    return HashTableSearchResult(nullptr);
  }

  bool HasValue() const { return value_addr_ != nullptr; };
  const ValueTy &Value() const { return *value_addr_; }
  ValueTy &Value() { return *value_addr_; }

private:
  HashTableSearchResult(ValueTy *value_addr) : value_addr_(value_addr) {}

  ValueTy *value_addr_{nullptr};
};

template <typename KeyTy> struct DefaultHashFn {
  size_t operator()(const KeyTy &key) const { return static_cast<size_t>(key); }
};

template <typename KeyTy, typename HashFn> struct DefaultSlotFn {
  size_t operator()(const KeyTy &key, size_t table_capacity) const {
    return HashFn{}(key) % table_capacity;
  }
};

class OptionalSlot {
public:
  OptionalSlot() : has_value_(false) {}
  OptionalSlot(size_t slot) : has_value_(true), slot_(slot) {}
  static OptionalSlot Nullopt() { return OptionalSlot{}; }

  bool HasValue() const { return has_value_; }
  size_t Value() const { return slot_; }

private:
  bool has_value_{false};
  size_t slot_{0ul};
};

/*
    Notes to add:

    - probing strategy
    - restrictions on key type
    - once inserted, access to value must be serialized externally
*/
template <typename KeyTy, typename ValueTy, KeyTy EmptyKey, KeyTy TombstoneKey,
          typename SlotFn = DefaultSlotFn<KeyTy, DefaultHashFn<KeyTy>>>
class AtomicHashTable {
  using AtomicKeyTy = typename AtomicKey<KeyTy>::Type;

public:
  AtomicHashTable(size_t capacity) {
    using namespace __sanitizer;
    keys_.Resize(capacity);
    for (auto n = 0u; n < keys_.Size(); ++n)
      atomic_store(&keys_[n], EmptyKey, memory_order::memory_order_release);

    values_.Resize(capacity);
  }

  size_t Capacity() const { return keys_.Size(); }
  size_t ApproxSize() const {
    using namespace __sanitizer;
    return atomic_load(&approx_size_, memory_order::memory_order_relaxed);
  }

  template <typename U>
  HashTableInsertResult Insert(const KeyTy &key, U &&value) {
    OptionalSlot slot = TryClaimInsertSlot(key);
    if (!slot.HasValue())
      return HashTableInsertResult::Overflow;

    using namespace __sanitizer;
    atomic_fetch_add(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
    values_[slot.Value()] = std::forward<U>(value);
    return HashTableInsertResult::OK;
  }

  HashTableRemoveResult Remove(const KeyTy &key) {
    OptionalSlot slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return HashTableRemoveResult::NotFound;

    TombstoneSlot(slot.Value());
    using namespace __sanitizer;
    // TODO do we need a zero check here?
    atomic_fetch_sub(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
    return HashTableRemoveResult::Removed;
  }

  HashTableSearchResult<ValueTy> Search(const KeyTy &key) {
    using Result = HashTableSearchResult<ValueTy>;
    OptionalSlot slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return Result::NotFound();
    return Result::Found(&values_[slot.Value()]);
  }

  // TODO remove duplication
  HashTableSearchResult<const ValueTy> Search(const KeyTy &key) const {
    using Result = HashTableSearchResult<const ValueTy>;
    OptionalSlot slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return Result::NotFound();
    return Result::Found(&values_[slot.Value()]);
  }

private:
  size_t TargetHashSlot(const KeyTy &key) const {
    return slot_fn_(key, Capacity());
  }

  size_t NextSlotLinearProbe(size_t current_slot) const {
    return (current_slot + 1ul) % Capacity();
  }

  OptionalSlot TryClaimInsertSlot(const KeyTy &key) {
    size_t slot = TargetHashSlot(key);
    for (size_t n = 0u; n < Capacity(); ++n) {
      if (SlotKey(slot) == key) {
        return OptionalSlot(slot);
      } else if (IsEmptySlot(slot) && ClaimEmptySlot(slot, key)) {
        return OptionalSlot(slot);
      }
      slot = NextSlotLinearProbe(slot);
    }
    return OptionalSlot::Nullopt();
  }

  OptionalSlot SearchForKeySlot(const KeyTy &key) const {
    size_t slot = TargetHashSlot(key);

    for (size_t n = 0u; n < Capacity(); ++n) {
      KeyTy slot_key = SlotKey(slot);

      if (slot_key == key)
        return OptionalSlot(slot);
      if (slot_key == EmptyKey)
        return OptionalSlot::Nullopt();

      slot = NextSlotLinearProbe(slot);
    }

    return OptionalSlot::Nullopt();
  }

  bool ClaimEmptySlot(size_t slot, const KeyTy &desired_key) {
    using namespace __sanitizer;
    KeyTy expected_key{EmptyKey};
    return atomic_compare_exchange_strong(&keys_[slot], &expected_key,
                                          desired_key,
                                          memory_order::memory_order_acq_rel);
  }

  void TombstoneSlot(size_t slot) {
    using namespace __sanitizer;
    atomic_store(&keys_[slot], TombstoneKey,
                 memory_order::memory_order_release);
  }

  bool IsEmptySlot(size_t slot) const { return SlotKey(slot) == EmptyKey; }

  KeyTy SlotKey(size_t slot) const {
    using namespace __sanitizer;
    return atomic_load(&keys_[slot], memory_order::memory_order_acquire);
  }

  const SlotFn slot_fn_{};
  __sanitizer::Vector<AtomicKeyTy> keys_;
  __sanitizer::Vector<ValueTy> values_; // TODO alignment for no false sharing
  __sanitizer::atomic_uint64_t approx_size_{0ul};
};

} // namespace __rtsan
