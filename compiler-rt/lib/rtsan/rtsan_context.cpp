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

#include <new>
#include <pthread.h>

using namespace __sanitizer;
using namespace __rtsan;

using ContextStorage = DenseMap<pthread_t, Context>;
static ContextStorage *context_storage = nullptr;

static void InitializeContextStorage() {
  static constexpr size_t max_supported_num_threads = 4096;
  context_storage =
      static_cast<ContextStorage *>(InternalAlloc(sizeof(ContextStorage)));
  new (context_storage) ContextStorage(max_supported_num_threads);
}

static __rtsan::Context &GetContextForThisThreadImpl() {
  if (context_storage == nullptr)
    InitializeContextStorage();

  pthread_t const thread_id = pthread_self();
  if (context_storage->contains(thread_id))
    return context_storage->find(thread_id)->getSecond();

  auto const [bucket_ptr, was_inserted] =
      context_storage->try_emplace(pthread_self(), Context{});
  return bucket_ptr->getSecond();
}

void __rtsan::Context::RealtimePush() { realtime_depth_++; }

void __rtsan::Context::RealtimePop() { realtime_depth_--; }

void __rtsan::Context::BypassPush() { bypass_depth_++; }

void __rtsan::Context::BypassPop() { bypass_depth_--; }

bool __rtsan::Context::InRealtimeContext() const { return realtime_depth_ > 0; }

bool __rtsan::Context::IsBypassed() const { return bypass_depth_ > 0; }

Context &__rtsan::GetContextForThisThread() {
  return GetContextForThisThreadImpl();
}
