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

} // namespace detail

namespace radsan {

Context::Context() : Context(createErrorActionGetter()) {}

Context::Context(std::function<OnErrorAction()> get_error_action)
    : get_error_action_(std::move(get_error_action)) {}

void Context::realtimePush() { realtime_depth_++; }

void Context::realtimePop() { realtime_depth_--; }

void Context::bypassPush() { bypass_depth_++; }

void Context::bypassPop() { bypass_depth_--; }

void Context::expectNotRealtime(const char *intercepted_function_name) {
  if (inRealtimeContext() && !isBypassed()) {
    bypassPush();
    printDiagnostics(intercepted_function_name);
    if (get_error_action_() == OnErrorAction::ExitWithFailure) {
      exit(EXIT_FAILURE);
    }
    bypassPop();
  }
}

bool Context::inRealtimeContext() const { return realtime_depth_ > 0; }

bool Context::isBypassed() const { return bypass_depth_ > 0; }

void Context::printDiagnostics(const char *intercepted_function_name) {
  fprintf(stderr,
          "Intercepted call to realtime-unsafe function `%s` from realtime "
          "context! Stack trace:\n",
          intercepted_function_name);
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
    *ptr = Context{};
    pthread_setspecific(detail::key, ptr);
  }

  return *ptr;
}

} // namespace radsan
