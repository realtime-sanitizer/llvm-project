/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan/radsan_interceptors.h"

#include "sanitizer_common/sanitizer_platform.h"

#include "interception/interception.h"
#include "radsan/radsan_context.h"

#if !SANITIZER_LINUX && !SANITIZER_APPLE
#error Sorry, radsan does not yet support this platform
#endif

#if SANITIZER_APPLE
#include <libkern/OSAtomic.h>
#include <os/lock.h>
#endif

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

using namespace __sanitizer;

namespace radsan {
void expectNotRealtime(const char *intercepted_function_name) {
  getContextForThisThread().expectNotRealtime(intercepted_function_name);
}
} // namespace radsan

/*
    Filesystem
*/

INTERCEPTOR(int, open, const char *path, int oflag, ...) {
  // TODO Establish whether we should intercept here if the flag contains
  // O_NONBLOCK
  radsan::expectNotRealtime("open");
  va_list args;
  va_start(args, oflag);
  auto result = REAL(open)(path, oflag, args);
  va_end(args);
  return result;
}

INTERCEPTOR(int, openat, int fd, const char *path, int oflag, ...) {
  // TODO Establish whether we should intercept here if the flag contains
  // O_NONBLOCK
  radsan::expectNotRealtime("openat");
  va_list args;
  va_start(args, oflag);
  auto result = REAL(openat)(fd, path, oflag, args);
  va_end(args);
  return result;
}

INTERCEPTOR(int, creat, const char *path, mode_t mode) {
  // TODO Establish whether we should intercept here if the flag contains
  // O_NONBLOCK
  radsan::expectNotRealtime("creat");
  auto result = REAL(creat)(path, mode);
  return result;
}

INTERCEPTOR(int, fcntl, int filedes, int cmd, ...) {
  radsan::expectNotRealtime("fcntl");
  va_list args;
  va_start(args, cmd);
  auto result = REAL(fcntl)(filedes, cmd, args);
  va_end(args);
  return result;
}

INTERCEPTOR(int, close, int filedes) {
  radsan::expectNotRealtime("close");
  return REAL(close)(filedes);
}

INTERCEPTOR(FILE *, fopen, const char *path, const char *mode) {
  radsan::expectNotRealtime("fopen");
  return REAL(fopen)(path, mode);
}

INTERCEPTOR(size_t, fread, void *ptr, size_t size, size_t nitems,
            FILE *stream) {
  radsan::expectNotRealtime("fread");
  return REAL(fread)(ptr, size, nitems, stream);
}

INTERCEPTOR(size_t, fwrite, const void *ptr, size_t size, size_t nitems,
            FILE *stream) {
  radsan::expectNotRealtime("fwrite");
  return REAL(fwrite)(ptr, size, nitems, stream);
}

INTERCEPTOR(int, fclose, FILE *stream) {
  radsan::expectNotRealtime("fclose");
  return REAL(fclose)(stream);
}

INTERCEPTOR(int, fputs, const char *s, FILE *stream) {
  radsan::expectNotRealtime("fputs");
  return REAL(fputs)(s, stream);
}

/*
    Streams
*/

INTERCEPTOR(int, puts, const char *s) {
  radsan::expectNotRealtime("puts");
  return REAL(puts)(s);
}

/*
    Concurrency
*/

#if SANITIZER_APPLE
#pragma clang diagnostic push
// OSSpinLockLock is deprecated, but still in use in libc++
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
INTERCEPTOR(void, OSSpinLockLock, volatile OSSpinLock *lock) {
  radsan::expectNotRealtime("OSSpinLockLock");
  return REAL(OSSpinLockLock)(lock);
}
#pragma clang diagnostic pop

INTERCEPTOR(void, os_unfair_lock_lock, os_unfair_lock_t lock) {
  radsan::expectNotRealtime("os_unfair_lock_lock");
  return REAL(os_unfair_lock_lock)(lock);
}
#elif SANITIZER_LINUX
INTERCEPTOR(int, pthread_spin_lock, pthread_spinlock_t *spinlock) {
  radsan::expectNotRealtime("pthread_spin_lock");
  return REAL(pthread_spin_lock)(spinlock);
}
#endif

INTERCEPTOR(int, pthread_create, pthread_t *thread, const pthread_attr_t *attr,
            void *(*start_routine)(void *), void *arg) {
  radsan::expectNotRealtime("pthread_create");
  return REAL(pthread_create)(thread, attr, start_routine, arg);
}

INTERCEPTOR(int, pthread_mutex_lock, pthread_mutex_t *mutex) {
  radsan::expectNotRealtime("pthread_mutex_lock");
  return REAL(pthread_mutex_lock)(mutex);
}

INTERCEPTOR(int, pthread_mutex_unlock, pthread_mutex_t *mutex) {
  radsan::expectNotRealtime("pthread_mutex_unlock");
  return REAL(pthread_mutex_unlock)(mutex);
}

INTERCEPTOR(int, pthread_join, pthread_t thread, void **value_ptr) {
  radsan::expectNotRealtime("pthread_join");
  return REAL(pthread_join)(thread, value_ptr);
}

INTERCEPTOR(int, pthread_cond_signal, pthread_cond_t *cond) {
  radsan::expectNotRealtime("pthread_cond_signal");
  return REAL(pthread_cond_signal)(cond);
}

INTERCEPTOR(int, pthread_cond_broadcast, pthread_cond_t *cond) {
  radsan::expectNotRealtime("pthread_cond_broadcast");
  return REAL(pthread_cond_broadcast)(cond);
}

INTERCEPTOR(int, pthread_cond_wait, pthread_cond_t *cond,
            pthread_mutex_t *mutex) {
  radsan::expectNotRealtime("pthread_cond_wait");
  return REAL(pthread_cond_wait)(cond, mutex);
}

INTERCEPTOR(int, pthread_cond_timedwait, pthread_cond_t *cond,
            pthread_mutex_t *mutex, const timespec *ts) {
  radsan::expectNotRealtime("pthread_cond_timedwait");
  return REAL(pthread_cond_timedwait)(cond, mutex, ts);
}

INTERCEPTOR(int, pthread_rwlock_rdlock, pthread_rwlock_t *lock) {
  radsan::expectNotRealtime("pthread_rwlock_rdlock");
  return REAL(pthread_rwlock_rdlock)(lock);
}

INTERCEPTOR(int, pthread_rwlock_unlock, pthread_rwlock_t *lock) {
  radsan::expectNotRealtime("pthread_rwlock_unlock");
  return REAL(pthread_rwlock_unlock)(lock);
}

INTERCEPTOR(int, pthread_rwlock_wrlock, pthread_rwlock_t *lock) {
  radsan::expectNotRealtime("pthread_rwlock_wrlock");
  return REAL(pthread_rwlock_wrlock)(lock);
}

/*
    Sleeping
*/

INTERCEPTOR(unsigned int, sleep, unsigned int s) {
  radsan::expectNotRealtime("sleep");
  return REAL(sleep)(s);
}

INTERCEPTOR(int, usleep, useconds_t u) {
  radsan::expectNotRealtime("usleep");
  return REAL(usleep)(u);
}

INTERCEPTOR(int, nanosleep, const struct timespec *rqtp,
            struct timespec *rmtp) {
  radsan::expectNotRealtime("nanosleep");
  return REAL(nanosleep)(rqtp, rmtp);
}

/*
    Memory
*/

INTERCEPTOR(void *, calloc, SIZE_T num, SIZE_T size) {
  radsan::expectNotRealtime("calloc");
  return REAL(calloc)(num, size);
}

INTERCEPTOR(void, free, void *ptr) {
  if (ptr != NULL) {
    radsan::expectNotRealtime("free");
  }
  return REAL(free)(ptr);
}

INTERCEPTOR(void *, malloc, SIZE_T size) {
  radsan::expectNotRealtime("malloc");
  return REAL(malloc)(size);
}

INTERCEPTOR(void *, realloc, void *ptr, SIZE_T size) {
  radsan::expectNotRealtime("realloc");
  return REAL(realloc)(ptr, size);
}

INTERCEPTOR(void *, reallocf, void *ptr, SIZE_T size) {
  radsan::expectNotRealtime("reallocf");
  return REAL(reallocf)(ptr, size);
}

INTERCEPTOR(void *, valloc, SIZE_T size) {
  radsan::expectNotRealtime("valloc");
  return REAL(valloc)(size);
}

#if SANITIZER_INTERCEPT_ALIGNED_ALLOC
INTERCEPTOR(void *, aligned_alloc, SIZE_T alignment, SIZE_T size) {
  radsan::expectNotRealtime("aligned_alloc");
  return REAL(aligned_alloc)(alignment, size);
}
#define RADSAN_MAYBE_INTERCEPT_ALIGNED_ALLOC INTERCEPT_FUNCTION(aligned_alloc)
#else
#define RADSAN_MAYBE_INTERCEPT_ALIGNED_ALLOC
#endif

INTERCEPTOR(int, posix_memalign, void **memptr, size_t alignment, size_t size) {
  radsan::expectNotRealtime("posix_memalign");
  return REAL(posix_memalign)(memptr, alignment, size);
}

/*
    Sockets
*/

INTERCEPTOR(int, socket, int domain, int type, int protocol) {
  radsan::expectNotRealtime("socket");
  return REAL(socket)(domain, type, protocol);
}

INTERCEPTOR(ssize_t, send, int sockfd, const void *buf, size_t len, int flags) {
  radsan::expectNotRealtime("send");
  return REAL(send)(sockfd, buf, len, flags);
}

INTERCEPTOR(ssize_t, sendmsg, int socket, const struct msghdr *message,
            int flags) {
  radsan::expectNotRealtime("sendmsg");
  return REAL(sendmsg)(socket, message, flags);
}

INTERCEPTOR(ssize_t, sendto, int socket, const void *buffer, size_t length,
            int flags, const struct sockaddr *dest_addr, socklen_t dest_len) {
  radsan::expectNotRealtime("sendto");
  return REAL(sendto)(socket, buffer, length, flags, dest_addr, dest_len);
}

INTERCEPTOR(ssize_t, recv, int socket, void *buffer, size_t length, int flags) {
  radsan::expectNotRealtime("recv");
  return REAL(recv)(socket, buffer, length, flags);
}

INTERCEPTOR(ssize_t, recvfrom, int socket, void *buffer, size_t length,
            int flags, struct sockaddr *address, socklen_t *address_len) {
  radsan::expectNotRealtime("recvfrom");
  return REAL(recvfrom)(socket, buffer, length, flags, address, address_len);
}

INTERCEPTOR(ssize_t, recvmsg, int socket, struct msghdr *message, int flags) {
  radsan::expectNotRealtime("recvmsg");
  return REAL(recvmsg)(socket, message, flags);
}

INTERCEPTOR(int, shutdown, int socket, int how) {
  radsan::expectNotRealtime("shutdown");
  return REAL(shutdown)(socket, how);
}

/*
    Preinit
*/

namespace radsan {
void initialiseInterceptors() {
  INTERCEPT_FUNCTION(calloc);
  INTERCEPT_FUNCTION(free);
  INTERCEPT_FUNCTION(malloc);
  INTERCEPT_FUNCTION(realloc);
  INTERCEPT_FUNCTION(reallocf);
  INTERCEPT_FUNCTION(valloc);
  RADSAN_MAYBE_INTERCEPT_ALIGNED_ALLOC;
  INTERCEPT_FUNCTION(posix_memalign);

  INTERCEPT_FUNCTION(open);
  INTERCEPT_FUNCTION(openat);
  INTERCEPT_FUNCTION(close);
  INTERCEPT_FUNCTION(fopen);
  INTERCEPT_FUNCTION(fread);
  INTERCEPT_FUNCTION(fwrite);
  INTERCEPT_FUNCTION(fclose);
  INTERCEPT_FUNCTION(fcntl);
  INTERCEPT_FUNCTION(creat);
  INTERCEPT_FUNCTION(puts);
  INTERCEPT_FUNCTION(fputs);

#if SANITIZER_APPLE
  INTERCEPT_FUNCTION(OSSpinLockLock);
  INTERCEPT_FUNCTION(os_unfair_lock_lock);
#elif SANITIZER_LINUX
  INTERCEPT_FUNCTION(pthread_spin_lock);
#endif

  INTERCEPT_FUNCTION(pthread_create);
  INTERCEPT_FUNCTION(pthread_mutex_lock);
  INTERCEPT_FUNCTION(pthread_mutex_unlock);
  INTERCEPT_FUNCTION(pthread_join);
  INTERCEPT_FUNCTION(pthread_cond_signal);
  INTERCEPT_FUNCTION(pthread_cond_broadcast);
  INTERCEPT_FUNCTION(pthread_cond_wait);
  INTERCEPT_FUNCTION(pthread_cond_timedwait);
  INTERCEPT_FUNCTION(pthread_rwlock_rdlock);
  INTERCEPT_FUNCTION(pthread_rwlock_unlock);
  INTERCEPT_FUNCTION(pthread_rwlock_wrlock);

  INTERCEPT_FUNCTION(sleep);
  INTERCEPT_FUNCTION(usleep);
  INTERCEPT_FUNCTION(nanosleep);

  INTERCEPT_FUNCTION(socket);
  INTERCEPT_FUNCTION(send);
  INTERCEPT_FUNCTION(sendmsg);
  INTERCEPT_FUNCTION(sendto);
  INTERCEPT_FUNCTION(recv);
  INTERCEPT_FUNCTION(recvmsg);
  INTERCEPT_FUNCTION(recvfrom);
  INTERCEPT_FUNCTION(shutdown);
}
} // namespace radsan
