/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan.h"
#include <radsan/radsan_context.h>

#include <radsan/radsan_stack.h>

#include <sanitizer_common/sanitizer_allocator_internal.h>
#include <sanitizer_common/sanitizer_stacktrace.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

using namespace __sanitizer;

namespace detail {

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
void internalFree(void *ptr) { InternalFree(ptr); }

static __sanitizer::atomic_uint64_t radsan_report_count{};

void IncrementReportCount();
} // namespace detail

namespace radsan {

__sanitizer::u64 GetReportCount() { 
  return __sanitizer::atomic_load(&detail::radsan_report_count, memory_order_acquire); 
}

void IncrementReportCount() { 
  __sanitizer::atomic_fetch_add(&detail::radsan_report_count, 1, memory_order_relaxed); 
}

Context::Context() : Context(createErrorActionGetter()) {}

Context::Context(std::function<OnErrorAction()> get_error_action)
    : get_error_action_(get_error_action) {}

void Context::realtimePush() { realtime_depth_++; }

void Context::realtimePop() { realtime_depth_--; }

void Context::bypassPush() { bypass_depth_++; }

void Context::bypassPop() { bypass_depth_--; }

void Context::expectNotRealtime(const char *intercepted_function_name) {
  CHECK(radsan_is_initialized());
  if (inRealtimeContext() && !isBypassed()) {
    bypassPush();
    printDiagnostics(intercepted_function_name);
    if (get_error_action_() == OnErrorAction::ExitWithFailure) {
      exit(common_flags()->exitcode);
    }
    bypassPop();
  }
}

bool Context::inRealtimeContext() const { return realtime_depth_ > 0; }

bool Context::isBypassed() const { return bypass_depth_ > 0; }

void Context::printDiagnostics(const char *intercepted_function_name) {
  fprintf(stderr,
          "Real-time violation: intercepted call to real-time unsafe function "
          "`%s` in real-time context! Stack trace:\n",
          intercepted_function_name);

  radsan::IncrementReportCount();
  radsan::printStackTrace();
}

Context &getContextForThisThread() {
  auto make_tls_key = []() {
    CHECK_EQ(pthread_key_create(&detail::key, detail::internalFree), 0);
  };

  pthread_once(&detail::key_once, make_tls_key);
  auto *ptr = static_cast<Context *>(pthread_getspecific(detail::key));
  if (ptr == nullptr) {
    ptr = static_cast<Context *>(InternalAlloc(sizeof(Context)));
    new(ptr) Context();
    pthread_setspecific(detail::key, ptr);
  }

  return *ptr;
}

} // namespace radsan
