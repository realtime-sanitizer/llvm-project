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

#include "sanitizer_common/sanitizer_dense_map.h"
#include "sanitizer_common/sanitizer_mutex.h"
#include <pthread.h>

namespace __rtsan {

class Context {
public:
  void RealtimePush();
  void RealtimePop();

  void BypassPush();
  void BypassPop();

  bool InRealtimeContext() const;
  bool IsBypassed() const;

  static Context &get();

private:
  static constexpr int max_concurrent_threads_{4096};
  struct Depth {
    int realtime{0};
    int bypass{0};
  };

  __sanitizer::DenseMap<pthread_t, Depth> depths_{max_concurrent_threads_};
  mutable __sanitizer::SpinMutex spin_mutex_;
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
