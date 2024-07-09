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

static pthread_key_t Key;
static pthread_once_t KeyOnce = PTHREAD_ONCE_INIT;
void internalFree(void *Ptr) { __sanitizer::InternalFree(Ptr); }

using radsan::Context;

Context &GetContextForThisThreadImpl() {
  auto MakeTlsKey = []() {
    CHECK_EQ(pthread_key_create(&detail::Key, detail::internalFree), 0);
  };

  pthread_once(&detail::KeyOnce, MakeTlsKey);
  Context *CurrentThreadContext =
      static_cast<Context *>(pthread_getspecific(detail::Key));
  if (CurrentThreadContext == nullptr) {
    CurrentThreadContext =
        static_cast<Context *>(InternalAlloc(sizeof(Context)));
    new (CurrentThreadContext) Context();
    pthread_setspecific(detail::Key, CurrentThreadContext);
  }

  return *CurrentThreadContext;
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

namespace radsan {

Context::Context() = default;

void Context::RealtimePush() { RealtimeDepth++; }

void Context::RealtimePop() { RealtimeDepth--; }

void Context::BypassPush() { BypassDepth++; }

void Context::BypassPop() { BypassDepth--; }

void Context::ExpectNotRealtime(const char *InterceptedFunctionName) {
  if (InRealtimeContext() && !IsBypassed()) {
    BypassPush();
    PrintDiagnostics(InterceptedFunctionName);
    detail::InvokeViolationDetectedAction();
    BypassPop();
  }
}

bool Context::InRealtimeContext() const { return RealtimeDepth > 0; }

bool Context::IsBypassed() const { return BypassDepth > 0; }

void Context::PrintDiagnostics(const char *InterceptedFunctionName) {
  fprintf(stderr,
          "Real-time violation: intercepted call to real-time unsafe function "
          "`%s` in real-time context! Stack trace:\n",
          InterceptedFunctionName);
  radsan::PrintStackTrace();
}

Context &GetContextForThisThread() {
  return detail::GetContextForThisThreadImpl();
}

} // namespace radsan
