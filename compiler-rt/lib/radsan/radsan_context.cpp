#include <radsan/radsan_context.h>

#include <radsan/radsan_stack.h>

#include <sanitizer_common/sanitizer_allocator_internal.h>
#include <sanitizer_common/sanitizer_stacktrace.h>

#include <stdio.h>
#include <stdlib.h>

using namespace __sanitizer;

namespace detail {
    static pthread_key_t key;
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
} // namespace detail

namespace radsan {
void Context::realtimePush() { realtime_depth_++; }
void Context::realtimePop() { realtime_depth_--; }
void Context::abortIfRealtime(const char* intercepted_function_name) {
    if (inRealtimeContext() && notAlreadyAborting()) {
        initiateAbort();
        printDiagnostics(intercepted_function_name);
        abort();
    }
}

bool Context::inRealtimeContext() const { return realtime_depth_ > 0; }
bool Context::notAlreadyAborting() const { return !already_aborting_; }
void Context::initiateAbort() { already_aborting_ = true; }
void Context::printDiagnostics(const char * intercepted_function_name) {
    printf("Intercepted call to realtime-unsafe function `%s` from realtime "
           "context! Stack trace\n", intercepted_function_name);
    radsan::printStackTrace();
    printf("Aborting.\n");
}

Context &getContextForThisThread() {
    auto make_tls_key = [](){
        CHECK_EQ(pthread_key_create(&detail::key, nullptr), 0);
    };

    pthread_once(&detail::key_once, make_tls_key);
    auto *ptr = static_cast<Context*>(pthread_getspecific(detail::key));
    if (ptr == nullptr) {
        ptr = static_cast<Context*>(InternalAlloc(sizeof(Context)));
        pthread_setspecific(detail::key, ptr);
    }

    return *ptr;
}
}


