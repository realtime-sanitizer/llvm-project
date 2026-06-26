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

#include "sanitizer_common/sanitizer_vector.h"

#include "rtsan/rtsan_atomic_hash_table_types.h"
#include "rtsan/rtsan_expected.h"
#include "rtsan/rtsan_optional.h"

namespace __rtsan {

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
  enum class InsertError { AlreadyExists, Overflow };
  enum class RemoveError { NotFound };
  enum class SearchError { NotFound };

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
  Expected<void, InsertError> Insert(const KeyTy &key, U &&value) {
    const auto slot = TryClaimInsertSlot(key);
    if (!slot.HasValue()) {
      switch (slot.Error()) { // TODO put in a fn
      case TryClaimSlotError::AlreadyExists:
        return Unexpected(InsertError::AlreadyExists);
      case TryClaimSlotError::Overflow:
        return Unexpected(InsertError::Overflow);
      }
    }

    using namespace __sanitizer;
    atomic_fetch_add(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
    values_[slot.Value()] = static_cast<U &&>(value);
    return {};
  }

  // TODO can we get a function like `operator[]`? I.e. searches and if it
  // doesn't find anything, it claims an insert slot with a default-constructed
  // value?

  Expected<void, RemoveError> Remove(const KeyTy &key) {
    Optional<SlotIndex> slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return Unexpected(RemoveError::NotFound);

    // TODO would it make sense to set the value to {} at this point?

    StoreSlotKey(slot.Value(), TombstoneKey);
    using namespace __sanitizer;
    // TODO do we need a zero check here?
    atomic_fetch_sub(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
    return {};
  }

  // TODO test failures
  Expected<ValueTy *, SearchError> Search(const KeyTy &key) {
    Optional<SlotIndex> slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return Unexpected(SearchError::NotFound);
    return &values_[slot.Value()];
  }

  // TODO test failures
  Expected<const ValueTy *, SearchError> Search(const KeyTy &key) const {
    Optional<SlotIndex> slot = SearchForKeySlot(key);
    if (!slot.HasValue())
      return Unexpected(SearchError::NotFound);
    return &values_[slot.Value()];
  }

private:
  enum class TryClaimSlotError { AlreadyExists, Overflow };
  enum class ClaimSlotError { FailedCompareExchange };

  SlotIndex TargetHashSlot(const KeyTy &key) const {
    return slot_fn_(key, Capacity());
  }

  SlotIndex NextSlotLinearProbe(SlotIndex current_slot) const {
    return (current_slot + 1ul) % Capacity();
  }

  Expected<SlotIndex, TryClaimSlotError>
  TryClaimInsertSlot(const KeyTy &desired_key) {

    SlotIndex slot = TargetHashSlot(desired_key);
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
        return Unexpected(TryClaimSlotError::AlreadyExists);

      if (IsTombstone(loaded_slot_key) && !first_tombstone_slot.HasValue())
        first_tombstone_slot = slot;

      slot = NextSlotLinearProbe(slot);
    }

    if (first_tombstone_slot.HasValue())
      return ClaimInsertSlotOrTryAgain(first_tombstone_slot.Value(),
                                       TombstoneKey, desired_key);

    return Unexpected(TryClaimSlotError::Overflow);
  }

  Optional<SlotIndex> SearchForKeySlot(const KeyTy &key) const {
    SlotIndex slot = TargetHashSlot(key);

    for (size_t probe_iter = 0u; probe_iter < Capacity(); ++probe_iter) {
      const KeyTy loaded_slot_key = LoadSlotKey(slot);

      if (loaded_slot_key == key)
        return slot;
      if (loaded_slot_key == EmptyKey)
        return Nullopt{};

      slot = NextSlotLinearProbe(slot);
    }

    return Nullopt{};
  }

  // TODO add max attempts fallback with appropriate error? Or just don't try
  // again and simply continue probing?
  Expected<SlotIndex, TryClaimSlotError>
  ClaimInsertSlotOrTryAgain(SlotIndex slot, const KeyTy &expected_key,
                            const KeyTy &desired_key) {
    const auto result = ClaimSlot(slot, expected_key, desired_key);

    if (!result.HasValue()) {
      switch (result.Error()) {
      case ClaimSlotError::FailedCompareExchange: // TODO is this the right
                                                  // thing to do?
        return TryClaimInsertSlot(desired_key);
      }
    }

    return result.Value();
  }

  Expected<SlotIndex, ClaimSlotError>
  ClaimSlot(SlotIndex slot, KeyTy expected_key, const KeyTy &desired_key) {
    using namespace __sanitizer;
    if (!atomic_compare_exchange_strong(&keys_[slot], &expected_key,
                                        desired_key,
                                        memory_order::memory_order_acq_rel))
      return Unexpected(ClaimSlotError::FailedCompareExchange);

    return slot;
  }

  bool IsEmpty(const KeyTy &key) { return key == EmptyKey; }
  bool IsTombstone(const KeyTy &key) { return key == TombstoneKey; }

  KeyTy LoadSlotKey(SlotIndex slot) const {
    using namespace __sanitizer;
    return atomic_load(&keys_[slot], memory_order::memory_order_acquire);
  }

  void StoreSlotKey(SlotIndex slot, const KeyTy &key) {
    using namespace __sanitizer;
    atomic_store(&keys_[slot], key, memory_order::memory_order_release);
  }

  const SlotFn slot_fn_{};
  __sanitizer::Vector<AtomicKeyTy> keys_;
  __sanitizer::Vector<ValueTy> values_; // TODO alignment for no false sharing
  __sanitizer::atomic_uint64_t approx_size_{0ul};
};

} // namespace __rtsan
