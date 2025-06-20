//===--- rtsan_context.cpp - Realtime Sanitizer -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "rtsan/rtsan_context.h"

#include "sanitizer_common/sanitizer_allocator_internal.h"
#include "sanitizer_common/sanitizer_dense_map.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

using namespace __sanitizer;
using namespace __rtsan;
inline void *operator new(size_t, void *ptr) noexcept { return ptr; }

static Context *context = nullptr;

static void InitializeContext() {
  context = static_cast<Context *>(InternalAlloc(sizeof(Context)));
  new (context) Context();
}

static __rtsan::Context &GetContextImpl() {
  if (context == nullptr)
    InitializeContext();
  return *context;
}

void __rtsan::Context::RealtimePush() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  depths_[pthread_self()].realtime++;
}

void __rtsan::Context::RealtimePop() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  depths_[pthread_self()].realtime--;
}

void __rtsan::Context::BypassPush() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  depths_[pthread_self()].bypass++;
}

void __rtsan::Context::BypassPop() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  depths_[pthread_self()].bypass--;
}

bool __rtsan::Context::InRealtimeContext() const {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  return depths_.lookup(pthread_self()).realtime > 0;
}

bool __rtsan::Context::IsBypassed() const {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  return depths_.lookup(pthread_self()).bypass > 0;
}

Context &__rtsan::GetContext() { return GetContextImpl(); }
