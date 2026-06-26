//===--- rtsan_context.h - Realtime Sanitizer -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "sanitizer_common/sanitizer_common.h"

#include "rtsan/rtsan_atomic_hash_table.h"

#include <stdint.h>

namespace __rtsan {

class Context {
public:
  Context();

  void RealtimePush();
  void RealtimePop();

  void BypassPush();
  void BypassPop();

  bool InRealtimeContext() const;
  bool IsBypassed() const;

private:
  struct Depths {
    int realtime{0};
    int bypass{0};
    bool operator==(Depths const &other) const {
      return realtime == other.realtime && bypass == other.bypass;
    }
  };

  using ThreadId = __sanitizer::ThreadID;

  /*
    Big TODO: in theory, these could collide with a valid ThreadId, which
    is a uint64_t. Maybe there is some way we can guarantee that the ThreadId
    isn't one of these values, or throw an error in the extremely unlikely case
    that there's a collision. Or, maybe, is there a way in which the idea
    of tagged keys (which will be required if we want to do any sort of
    tombstone reclamation) might be handy here? Perhaps we can lay out the
    atomic key type in the hash table as

        enum class KeyKind : public uint32_t
        {
            Valid,
            Empty,
            Tombstone
        };

        using KeyTag = uint32_t;   // result of atomic<uint32_t>::fetch_add

        // Total size 128 bits (small enough for double-width compare and swap)
        struct TaggedKey
        {
            Key key;               // 64 bits (ThreadId)
            KeyKind kind;          // 32 bits
            KeyTag tag;            // 32 bits
        };
  */
  struct TLSAtomicHashTableConfig {
    using KeyType = ThreadId;
    using ValueType = Depths;
    using SlotFn = DefaultSlotFn<KeyType, DefaultHashFn<KeyType>>;
    static constexpr KeyType EmptyKey = UINT64_MAX;
    static constexpr KeyType TombstoneKey = UINT64_MAX - 1u;
  };

  AtomicHashTable<TLSAtomicHashTableConfig> tls_;
};

class ScopedBypass {
public:
  [[nodiscard]] explicit ScopedBypass(Context &context) : context_(context) {
    context_.BypassPush();
  }

  ~ScopedBypass() { context_.BypassPop(); }

  ScopedBypass(const ScopedBypass &) = delete;
  ScopedBypass &operator=(const ScopedBypass &) = delete;
  ScopedBypass(ScopedBypass &&) = delete;
  ScopedBypass &operator=(ScopedBypass &&) = delete;

private:
  Context &context_;
};

Context &GetContext();
} // namespace __rtsan
