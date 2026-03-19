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

#include <stddef.h>
#include <stdint.h>

namespace __rtsan {

// TODO what is better? Using this or just using size_t? Be consistent
// throughout impl.
using SlotIndex = size_t;

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

enum class InsertResult { OK, AlreadyExists, Overflow };
enum class RemoveResult { Removed, NotFound };

template <typename KeyTy> struct DefaultHashFn {
  size_t operator()(const KeyTy &key) const { return static_cast<size_t>(key); }
};

template <typename KeyTy, typename HashFn> struct DefaultSlotFn {
  size_t operator()(const KeyTy &key, size_t table_capacity) const {
    return HashFn{}(key) % table_capacity;
  }
};

template <typename ValueTy> class Optional {
public:
  Optional() : has_value_(false) {}
  // TODO tighten up ref semantics
  Optional(ValueTy value) : has_value_(true), value_(value) {}
  static Optional Nullopt() { return Optional{}; }

  bool HasValue() const { return has_value_; }
  const ValueTy &Value() const { return value_; }

private:
  bool has_value_{false};
  ValueTy value_{};
};

template <typename ValueTy, typename ErrorTy> class Expected {
public:
  Expected(ValueTy value) : Expected(true, value, {}) {}
  static Expected Unexpected(ErrorTy error) {
    return Expected(false, {}, error);
  }

  bool HasValue() const { return has_value_; }
  const ValueTy &Value() const { return value_; }
  const ErrorTy &Error() const { return error_; }

private:
  Expected(bool has_value, ValueTy value, ErrorTy error)
      : has_value_(has_value), value_(value), error_(error) {}

  bool has_value_{false};
  ValueTy value_{};
  ErrorTy error_{};
};

enum class TryClaimInsertSlotError { AlreadyExists, Overflow };
using TryClaimInsertSlotResult = Expected<SlotIndex, TryClaimInsertSlotError>;

// TODO better name
enum class ClaimSlotError { FailedCompareExchange };
using ClaimSlotResult = Expected<SlotIndex, ClaimSlotError>;

enum class SearchError { NotFound };
template <typename ValueTy>
using SearchResult = Expected<ValueTy *, SearchError>;

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

/*
    Notes to add:

    - probing strategy
    - restrictions on key type
    - once inserted, access to value must be serialized externally
    - how tagged keys solve the ABA problem in tombstone reclamation (if
      we get there and confirm it)

    Big TODOs:

    - Do we need 128 bit keys to accommodate all 64-bit ThreadIDs?
    - tombstone reclamation, requiring:
        - tagged keys
        - proof that with tagged keys it's safe
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

  // TODO should InsertResult return the slot index as a value?
  template <typename U> InsertResult Insert(const KeyTy &key, U &&value) {
    TryClaimInsertSlotResult slot = TryClaimInsertSlot(key);
    if (!slot.HasValue()) {
      switch (slot.Error()) { // TODO put in a fn
      case TryClaimInsertSlotError::AlreadyExists:
        return InsertResult::AlreadyExists;
      case TryClaimInsertSlotError::Overflow:
        return InsertResult::Overflow;
      }
    }

    using namespace __sanitizer;
    atomic_fetch_add(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
    values_[slot.Value()] = static_cast<U&&>(value);
    return InsertResult::OK;
  }

  // TODO can we get a function like `operator[]`? I.e. searches and if it
  // doesn't find anything, it claims an insert slot with a default-constructed
  // value?

  RemoveResult Remove(const KeyTy &key) {
    Optional<SlotIndex> slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return RemoveResult::NotFound;

    // TODO would it make sense to set the value to {} at this point?

    StoreSlotKey(slot.Value(), TombstoneKey);
    using namespace __sanitizer;
    // TODO do we need a zero check here?
    atomic_fetch_sub(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
    return RemoveResult::Removed;
  }

  // TODO simplify to `earchResult<ValueTy>
  HashTableSearchResult<ValueTy> Search(const KeyTy &key) {
    using Result = HashTableSearchResult<ValueTy>;
    Optional<SlotIndex> slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return Result::NotFound();
    return Result::Found(&values_[slot.Value()]);
  }

  // TODO remove duplication and simplify to SearchResult<const ValueTy>
  HashTableSearchResult<const ValueTy> Search(const KeyTy &key) const {
    using Result = HashTableSearchResult<const ValueTy>;
    Optional<SlotIndex> slot = SearchForKeySlot(key);
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

  TryClaimInsertSlotResult TryClaimInsertSlot(const KeyTy &desired_key) {

    size_t slot = TargetHashSlot(desired_key);
    Optional<SlotIndex> first_tombstone_slot{};

    for (size_t probe_iter = 0u; probe_iter < Capacity(); ++probe_iter) {
      const KeyTy loaded_slot_key = LoadSlotKey(slot);

      if (IsEmpty(loaded_slot_key)) {
        if (first_tombstone_slot.HasValue()) {
          return ClaimInsertSlotOrTryAgain(first_tombstone_slot.Value(),
                                           TombstoneKey, desired_key);
        }
        return ClaimInsertSlotOrTryAgain(slot, EmptyKey, desired_key);
      }

      if (loaded_slot_key == desired_key)
        return TryClaimInsertSlotResult::Unexpected(
            TryClaimInsertSlotError::AlreadyExists);

      if (IsTombstone(loaded_slot_key) && !first_tombstone_slot.HasValue())
        first_tombstone_slot = slot;

      slot = NextSlotLinearProbe(slot);
    }

    if (first_tombstone_slot.HasValue())
      return ClaimInsertSlotOrTryAgain(first_tombstone_slot.Value(),
                                       TombstoneKey, desired_key);

    return TryClaimInsertSlotResult::Unexpected(
        TryClaimInsertSlotError::Overflow);
  }

  Optional<SlotIndex> SearchForKeySlot(const KeyTy &key) const {
    size_t slot = TargetHashSlot(key);

    for (size_t probe_iter = 0u; probe_iter < Capacity(); ++probe_iter) {
      const KeyTy loaded_slot_key = LoadSlotKey(slot);

      if (loaded_slot_key == key)
        return Optional<SlotIndex>(slot);
      if (loaded_slot_key == EmptyKey)
        return Optional<SlotIndex>::Nullopt();

      slot = NextSlotLinearProbe(slot);
    }

    return Optional<SlotIndex>::Nullopt();
  }

  // TODO add max attempts fallback with appropriate error? Or just don't try
  // again and simply continue probing?
  TryClaimInsertSlotResult ClaimInsertSlotOrTryAgain(size_t slot,
                                                     const KeyTy &expected_key,
                                                     const KeyTy &desired_key) {
    const ClaimSlotResult result = ClaimSlot(slot, expected_key, desired_key);

    if (!result.HasValue()) {
      switch (result.Error()) {
      case ClaimSlotError::FailedCompareExchange: // TODO is this the right
                                                  // thing to do?
        return TryClaimInsertSlot(desired_key);
      }
    }

    return TryClaimInsertSlotResult(result.Value());
  }

  ClaimSlotResult ClaimSlot(size_t slot, KeyTy expected_key,
                            const KeyTy &desired_key) {
    using namespace __sanitizer;
    const bool successfully_exchanged =
        atomic_compare_exchange_strong(&keys_[slot], &expected_key, desired_key,
                                       memory_order::memory_order_acq_rel);

    return successfully_exchanged ? ClaimSlotResult(slot)
                                  : ClaimSlotResult::Unexpected(
                                        ClaimSlotError::FailedCompareExchange);
  }

  bool IsEmpty(const KeyTy &key) { return key == EmptyKey; }
  bool IsTombstone(const KeyTy &key) { return key == TombstoneKey; }

  KeyTy LoadSlotKey(size_t slot) const {
    using namespace __sanitizer;
    return atomic_load(&keys_[slot], memory_order::memory_order_acquire);
  }

  void StoreSlotKey(size_t slot, const KeyTy &key) {
    using namespace __sanitizer;
    atomic_store(&keys_[slot], key, memory_order::memory_order_release);
  }

  const SlotFn slot_fn_{};
  __sanitizer::Vector<AtomicKeyTy> keys_;
  __sanitizer::Vector<ValueTy> values_; // TODO alignment for no false sharing
  __sanitizer::atomic_uint64_t approx_size_{0ul};
};

} // namespace __rtsan
