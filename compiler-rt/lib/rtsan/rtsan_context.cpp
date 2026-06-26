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
#include "sanitizer_common/sanitizer_placement_new.h"

#include <pthread.h>
#include <stddef.h>

using namespace __sanitizer;
using namespace __rtsan;

static Context *context = nullptr;

static void InitializeContext() {
  context = static_cast<Context *>(InternalAlloc(sizeof(Context)));
  new (context) Context();
}

static constexpr unsigned tls_capacity_ = 512u;

__rtsan::Context::Context() : tls_(tls_capacity_) {}

void __rtsan::Context::RealtimePush() {
  const ThreadId thread_id = GetTid();
  auto depths = tls_.Search(thread_id);
  if (depths.HasValue())
    depths.Value()->realtime++;
  else
    tls_.Insert(thread_id, Depths{.realtime = 1, .bypass = 0});
  // TODO handle failure
}

void __rtsan::Context::RealtimePop() {
  const ThreadId thread_id = GetTid();
  auto depths = tls_.Search(thread_id);
  if (depths.HasValue()) {
    depths.Value()->realtime--;
    // TODO removing this is pretty gnarly while depths is in scope
    if (*depths.Value() == Depths{0, 0})
      tls_.Remove(thread_id);
  }
  // TODO handle failure
}

void __rtsan::Context::BypassPush() {
  const ThreadId thread_id = GetTid();
  auto depths = tls_.Search(thread_id);
  if (depths.HasValue())
    depths.Value()->bypass++;
  else
    tls_.Insert(thread_id, Depths{.realtime = 0, .bypass = 1});
  // TODO handle failure
}

void __rtsan::Context::BypassPop() {
  const ThreadId thread_id = GetTid();
  auto depths = tls_.Search(thread_id);
  if (depths.HasValue()) {
    depths.Value()->bypass--;
    // TODO removing this is pretty gnarly while depths is in scope
    if (*depths.Value() == Depths{0, 0})
      tls_.Remove(thread_id);
  }
  // TODO handle failure
}

bool __rtsan::Context::InRealtimeContext() const {
  const auto depths = tls_.Search(GetTid());
  if (!depths.HasValue())
    return false;
  return depths.Value()->realtime > 0;
}

bool __rtsan::Context::IsBypassed() const {
  const auto depths = tls_.Search(GetTid());
  if (!depths.HasValue())
    return false;
  return depths.Value()->bypass > 0;
}

Context &__rtsan::GetContext() {
  if (context == nullptr) {
    InitializeContext();
  }
  return *context;
}
