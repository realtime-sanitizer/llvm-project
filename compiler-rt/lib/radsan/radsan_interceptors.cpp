#include <radsan/radsan_interceptors.h>

#include <sanitizer_common/sanitizer_platform.h>

#include <interception/interception.h>
#include <radsan/radsan_context.h>

#if !SANITIZER_LINUX && !SANITIZER_APPLE
#error Sorry, radsan does not yet support this platform
#endif

#if SANITIZER_APPLE
#include <libkern/OSAtomic.h>
#endif

#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

    using namespace __sanitizer;

namespace radsan {
void abortIfRealtime(const char *intercepted_function_name) {
  getContextForThisThread().abortIfRealtime(intercepted_function_name);
}
} // namespace radsan

/*
    Filesystem
*/

INTERCEPTOR(int, open, const char *path, int oflag, ...) {
  // TODO Establish whether we should intercept here if the flag contains
  // O_NONBLOCK
  radsan::abortIfRealtime("open");
  va_list args;
  va_start(args, oflag);
  auto result = REAL(open)(path, oflag, args);
  va_end(args);
  return result;
}

INTERCEPTOR(int, close, int filedes) {
  radsan::abortIfRealtime("close");
  return REAL(close)(filedes);
}

/*
    Concurrency
*/

#if SANITIZER_APPLE
INTERCEPTOR(void, OSSpinLockLock, volatile OSSpinLock *lock) {
  radsan::abortIfRealtime("OSSpinLockLock");
  return REAL(OSSpinLockLock)(lock);
}
#endif
// TODO spin lock stuff for linux: is it pthread_spin_lock and friends?

INTERCEPTOR(int, pthread_create, pthread_t *thread, const pthread_attr_t *attr,
            void *(*start_routine)(void *), void *arg) {
  radsan::abortIfRealtime("pthread_create");
  return REAL(pthread_create)(thread, attr, start_routine, arg);
}

INTERCEPTOR(int, pthread_mutex_lock, pthread_mutex_t *mutex) {
  radsan::abortIfRealtime("pthread_mutex_lock");
  return REAL(pthread_mutex_lock)(mutex);
}

INTERCEPTOR(int, pthread_mutex_unlock, pthread_mutex_t *mutex) {
  radsan::abortIfRealtime("pthread_mutex_unlock");
  return REAL(pthread_mutex_unlock)(mutex);
}

INTERCEPTOR(int, pthread_join, pthread_t thread, void **value_ptr) {
  radsan::abortIfRealtime("pthread_join");
  return REAL(pthread_join)(thread, value_ptr);
}

INTERCEPTOR(unsigned int, sleep, unsigned int s) {
  radsan::abortIfRealtime("sleep");
  return REAL(sleep)(s);
}

INTERCEPTOR(int, usleep, useconds_t u) {
  radsan::abortIfRealtime("usleep");
  return REAL(usleep)(u);
}

INTERCEPTOR(int, nanosleep, const struct timespec *rqtp,
            struct timespec *rmtp) {
  radsan::abortIfRealtime("nanosleep");
  return REAL(nanosleep)(rqtp, rmtp);
}

/*
    Memory
*/

INTERCEPTOR(void *, calloc, SIZE_T num, SIZE_T size) {
  radsan::abortIfRealtime("calloc");
  return REAL(calloc)(num, size);
}

INTERCEPTOR(void, free, void *ptr) {
  radsan::abortIfRealtime("free");
  return REAL(free)(ptr);
}

INTERCEPTOR(void *, malloc, SIZE_T size) {
  radsan::abortIfRealtime("malloc");
  return REAL(malloc)(size);
}

INTERCEPTOR(void *, realloc, void *ptr, SIZE_T size) {
  radsan::abortIfRealtime("realloc");
  return REAL(realloc)(ptr, size);
}

INTERCEPTOR(void *, reallocf, void *ptr, SIZE_T size) {
  radsan::abortIfRealtime("reallocf");
  return REAL(reallocf)(ptr, size);
}

INTERCEPTOR(void *, valloc, SIZE_T size) {
  radsan::abortIfRealtime("valloc");
  return REAL(valloc)(size);
}

INTERCEPTOR(void *, aligned_alloc, SIZE_T alignment, SIZE_T size) {
  radsan::abortIfRealtime("aligned_alloc");
  return REAL(aligned_alloc)(alignment, size);
}

/*
    Preinit
*/

namespace radsan {
void initialiseInterceptors()
{
  INTERCEPT_FUNCTION(open);
  INTERCEPT_FUNCTION(close);

#if SANITIZER_APPLE
  INTERCEPT_FUNCTION(OSSpinLockLock);
#endif

  INTERCEPT_FUNCTION(pthread_create);
  INTERCEPT_FUNCTION(pthread_mutex_lock);
  INTERCEPT_FUNCTION(pthread_mutex_unlock);
  INTERCEPT_FUNCTION(pthread_join);

  INTERCEPT_FUNCTION(sleep);
  INTERCEPT_FUNCTION(usleep);
  INTERCEPT_FUNCTION(nanosleep);

  INTERCEPT_FUNCTION(calloc);
  INTERCEPT_FUNCTION(free);
  INTERCEPT_FUNCTION(malloc);
  INTERCEPT_FUNCTION(realloc);
  INTERCEPT_FUNCTION(reallocf);
  INTERCEPT_FUNCTION(valloc);
  INTERCEPT_FUNCTION(aligned_alloc);
}
}
