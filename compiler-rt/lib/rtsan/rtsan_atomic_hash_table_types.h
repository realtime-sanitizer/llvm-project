//===--- rtsan_atomic_hash_table_types.h - Realtime Sanitizer ---*- C++ -*-===//
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

#include <stddef.h>

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

template <typename KeyTy> struct DefaultHashFn {
  size_t operator()(const KeyTy &key) const { return static_cast<size_t>(key); }
};

template <typename KeyTy, typename HashFn> struct DefaultSlotFn {
  SlotIndex operator()(const KeyTy &key, size_t table_capacity) const {
    return HashFn{}(key) % table_capacity;
  }
};

} // namespace __rtsan
