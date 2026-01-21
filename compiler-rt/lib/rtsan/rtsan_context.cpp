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
#include "sanitizer_common/sanitizer_placement_new.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

using namespace __sanitizer;
using namespace __rtsan;

static Context *context = nullptr;

static void InitializeContext() {
  context = static_cast<Context *>(InternalAlloc(sizeof(Context)));
  new (context) Context();
}

static constexpr unsigned init_depths_capacity = 512u;

__rtsan::Context::Context() : depths_(init_depths_capacity) {}

void __rtsan::Context::RealtimePush() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  depths_[pthread_self()].realtime++;
}

void __rtsan::Context::RealtimePop() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  pthread_t const thread_id = pthread_self();
  depths_[thread_id].realtime--;
  MaybeCleanup(thread_id);
}

void __rtsan::Context::BypassPush() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  depths_[pthread_self()].bypass++;
}

void __rtsan::Context::BypassPop() {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  pthread_t const thread_id = pthread_self();
  depths_[thread_id].bypass--;
  MaybeCleanup(thread_id);
}

bool __rtsan::Context::InRealtimeContext() const {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  return depths_.lookup(pthread_self()).realtime > 0;
}

bool __rtsan::Context::IsBypassed() const {
  __sanitizer::SpinMutexLock lock{&spin_mutex_};
  return depths_.lookup(pthread_self()).bypass > 0;
}

void __rtsan::Context::MaybeCleanup(pthread_t thread_id) {
  if (depths_[thread_id] == Depth{})
    depths_.erase(thread_id);
}

Context &__rtsan::GetContext() {
  if (context == nullptr) {
    InitializeContext();
  }
  return *context;
}
