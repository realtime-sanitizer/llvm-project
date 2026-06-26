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
template <typename HashTableConfig> class AtomicHashTable {
public:
  using KeyTy = typename HashTableConfig::KeyType;
  using ValueTy = typename HashTableConfig::ValueType;
  static constexpr KeyTy EmptyKey = HashTableConfig::EmptyKey;
  static constexpr KeyTy TombstoneKey = HashTableConfig::TombstoneKey;
  enum class InsertError { KeyAlreadyExists, Overflow };
  enum class RemoveError { KeyNotFound };
  enum class SearchError { KeyNotFound };

  AtomicHashTable(size_t capacity);

  size_t Capacity() const;
  size_t ApproxSize() const;

  template <typename U>
  Expected<void, InsertError> Insert(const KeyTy &key, U &&value);
  Expected<void, RemoveError> Remove(const KeyTy &key);

  Expected<ValueTy *, SearchError> Search(const KeyTy &key);
  Expected<const ValueTy *, SearchError> Search(const KeyTy &key) const;

  // TODO can we get a function like `operator[]`? I.e. searches and if it
  // doesn't find anything, it claims an insert slot with a default-constructed
  // value?

private:
  using AtomicKeyTy = typename AtomicKey<KeyTy>::Type;
  enum class TryClaimSlotError { AlreadyExists, Overflow };
  enum class ClaimSlotError { FailedCompareExchange };

  SlotIndex TargetHashSlot(const KeyTy &key) const;
  SlotIndex NextSlotLinearProbe(SlotIndex current_slot) const;

  Expected<SlotIndex, TryClaimSlotError>
  TryClaimInsertSlot(const KeyTy &desired_key);

  Optional<SlotIndex> SearchForKeySlot(const KeyTy &key) const;

  Expected<SlotIndex, TryClaimSlotError>
  ClaimInsertSlotOrTryAgain(SlotIndex slot, const KeyTy &expected_key,
                            const KeyTy &desired_key);

  Expected<SlotIndex, ClaimSlotError>
  ClaimSlot(SlotIndex slot, KeyTy expected_key, const KeyTy &desired_key);

  static bool IsEmpty(const KeyTy &key);
  static bool IsTombstone(const KeyTy &key);
  KeyTy LoadSlotKey(SlotIndex slot) const;
  void StoreSlotKey(SlotIndex slot, const KeyTy &key);

  const typename HashTableConfig::SlotFn slot_fn_{};
  __sanitizer::Vector<AtomicKeyTy> keys_;
  __sanitizer::Vector<ValueTy> values_; // TODO alignment for no false sharing
  __sanitizer::atomic_uint64_t approx_size_{0ul};
};

// Implementation details...

template <typename Config>
AtomicHashTable<Config>::AtomicHashTable(size_t capacity) {
  using namespace __sanitizer;
  keys_.Resize(capacity);
  for (auto n = 0u; n < keys_.Size(); ++n)
    atomic_store(&keys_[n], EmptyKey, memory_order::memory_order_release);

  values_.Resize(capacity);
}

template <typename Config> size_t AtomicHashTable<Config>::Capacity() const {
  return keys_.Size();
}

template <typename Config> size_t AtomicHashTable<Config>::ApproxSize() const {
  using namespace __sanitizer;
  return atomic_load(&approx_size_, memory_order::memory_order_relaxed);
}

template <typename Config>
template <typename U>
auto AtomicHashTable<Config>::Insert(const KeyTy &key, U &&value)
    -> Expected<void, InsertError> {
  const auto slot = TryClaimInsertSlot(key);
  if (!slot.HasValue()) {
    switch (slot.Error()) { // TODO put in a fn
    case TryClaimSlotError::AlreadyExists:
      return Unexpected(InsertError::KeyAlreadyExists);
    case TryClaimSlotError::Overflow:
      return Unexpected(InsertError::Overflow);
    }
  }

  using namespace __sanitizer;
  atomic_fetch_add(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
  values_[slot.Value()] = static_cast<U &&>(value);
  return {};
}

template <typename Config>
auto AtomicHashTable<Config>::Remove(const KeyTy &key)
    -> Expected<void, RemoveError> {
  Optional<SlotIndex> slot = SearchForKeySlot(key);
  if (!slot.HasValue())
    return Unexpected(RemoveError::KeyNotFound);

  // TODO would it make sense to set the value to {} at this point?

  StoreSlotKey(slot.Value(), TombstoneKey);
  using namespace __sanitizer;
  // TODO do we need a zero check here?
  atomic_fetch_sub(&approx_size_, 1ul, memory_order::memory_order_acq_rel);
  return {};
}

// TODO test failures
template <typename Config>
auto AtomicHashTable<Config>::Search(const KeyTy &key)
    -> Expected<ValueTy *, SearchError> {
  Optional<SlotIndex> slot = SearchForKeySlot(key);
  if (!slot.HasValue())
    return Unexpected(SearchError::KeyNotFound);
  return &values_[slot.Value()];
}

// TODO test failures
template <typename Config>
auto AtomicHashTable<Config>::Search(const KeyTy &key) const
    -> Expected<const ValueTy *, SearchError> {
  Optional<SlotIndex> slot = SearchForKeySlot(key);
  if (!slot.HasValue())
    return Unexpected(SearchError::KeyNotFound);
  return &values_[slot.Value()];
}

template <typename Config>
SlotIndex AtomicHashTable<Config>::TargetHashSlot(const KeyTy &key) const {
  return slot_fn_(key, Capacity());
}

template <typename Config>
SlotIndex
AtomicHashTable<Config>::NextSlotLinearProbe(SlotIndex current_slot) const {
  return (current_slot + 1ul) % Capacity();
}

template <typename Config>
auto AtomicHashTable<Config>::TryClaimInsertSlot(const KeyTy &desired_key)
    -> Expected<SlotIndex, TryClaimSlotError> {

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
    return ClaimInsertSlotOrTryAgain(first_tombstone_slot.Value(), TombstoneKey,
                                     desired_key);

  return Unexpected(TryClaimSlotError::Overflow);
}

template <typename Config>
Optional<SlotIndex>
AtomicHashTable<Config>::SearchForKeySlot(const KeyTy &key) const {
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
template <typename Config>
auto AtomicHashTable<Config>::ClaimInsertSlotOrTryAgain(
    SlotIndex slot, const KeyTy &expected_key, const KeyTy &desired_key)
    -> Expected<SlotIndex, TryClaimSlotError> {

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

template <typename Config>
auto AtomicHashTable<Config>::ClaimSlot(SlotIndex slot, KeyTy expected_key,
                                        const KeyTy &desired_key)
    -> Expected<SlotIndex, ClaimSlotError> {
  using namespace __sanitizer;
  if (!atomic_compare_exchange_strong(&keys_[slot], &expected_key, desired_key,
                                      memory_order::memory_order_acq_rel))
    return Unexpected(ClaimSlotError::FailedCompareExchange);

  return slot;
}

template <typename Config>
bool AtomicHashTable<Config>::IsEmpty(const KeyTy &key) {
  return key == EmptyKey;
}

template <typename Config>
bool AtomicHashTable<Config>::IsTombstone(const KeyTy &key) {
  return key == TombstoneKey;
}

template <typename Config>
auto AtomicHashTable<Config>::LoadSlotKey(SlotIndex slot) const -> KeyTy {
  using namespace __sanitizer;
  return atomic_load(&keys_[slot], memory_order::memory_order_acquire);
}

template <typename Config>
void AtomicHashTable<Config>::StoreSlotKey(SlotIndex slot, const KeyTy &key) {
  using namespace __sanitizer;
  atomic_store(&keys_[slot], key, memory_order::memory_order_release);
}

} // namespace __rtsan
