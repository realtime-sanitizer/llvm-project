/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include <radsan/radsan_context.h>

#include <radsan/radsan_stack.h>

#include <sanitizer_common/sanitizer_allocator_internal.h>
#include <sanitizer_common/sanitizer_stacktrace.h>

#include <new>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

using namespace __sanitizer;

namespace detail {

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
void internalFree(void *ptr) { __sanitizer::InternalFree(ptr); }

using __radsan::Context;

Context &GetContextForThisThreadImpl() {
  auto make_tls_key = []() {
    CHECK_EQ(pthread_key_create(&detail::key, detail::internalFree), 0);
  };

  pthread_once(&detail::key_once, make_tls_key);
  Context *current_thread_context =
      static_cast<Context *>(pthread_getspecific(detail::key));
  if (current_thread_context == nullptr) {
    current_thread_context =
        static_cast<Context *>(InternalAlloc(sizeof(Context)));
    new (current_thread_context) Context();
    pthread_setspecific(detail::key, current_thread_context);
  }

  return *current_thread_context;
}

/*
    This is a placeholder stub for a future feature that will allow
    a user to configure RADSan's behaviour when a real-time safety
    violation is detected. The RADSan developers intend for the
    following choices to be made available, via a RADSAN_OPTIONS
    environment variable, in a future PR:

        i) exit,
       ii) continue, or
      iii) wait for user input from stdin.

    Until then, and to keep the first PRs small, only the exit mode
    is available.
*/
void InvokeViolationDetectedAction() { exit(EXIT_FAILURE); }

} // namespace detail

namespace __radsan {

Context::Context() = default;

void Context::RealtimePush() { realtime_depth++; }

void Context::RealtimePop() { realtime_depth--; }

void Context::BypassPush() { bypass_depth++; }

void Context::BypassPop() { bypass_depth--; }

void Context::ExpectNotRealtime(const char *intercepted_function_name) {
  if (InRealtimeContext() && !IsBypassed()) {
    BypassPush();
    PrintDiagnostics(intercepted_function_name);
    detail::InvokeViolationDetectedAction();
    BypassPop();
  }
}

bool Context::InRealtimeContext() const { return realtime_depth > 0; }

bool Context::IsBypassed() const { return bypass_depth > 0; }

void Context::PrintDiagnostics(const char *intercepted_function_name) {
  fprintf(stderr,
          "Real-time violation: intercepted call to real-time unsafe function "
          "`%s` in real-time context! Stack trace:\n",
          intercepted_function_name);
  __radsan::PrintStackTrace();
}

Context &GetContextForThisThread() {
  return detail::GetContextForThisThreadImpl();
}

} // namespace __radsan
