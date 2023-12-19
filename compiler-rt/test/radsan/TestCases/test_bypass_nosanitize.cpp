// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <pthread.h>
#include <stdlib.h>

__attribute__((no_sanitize("realtime")))
void bypassedLock(pthread_mutex_t& Mutex) {
  pthread_mutex_lock(&Mutex);
}

__attribute__((no_sanitize("realtime")))
void bypassedUnlock(pthread_mutex_t& Mutex) {
    pthread_mutex_unlock(&Mutex);
}

void violationLock(pthread_mutex_t& Mutex) {
  pthread_mutex_lock(&Mutex);
}

[[clang::realtime]] void process(pthread_mutex_t& Mutex) {
  bypassedLock(Mutex);
  bypassedUnlock(Mutex);

  violationLock(Mutex);
  bypassedUnlock(Mutex);
}

int main() {
  pthread_mutex_t Mutex;
  pthread_mutex_init(&Mutex, NULL);

  process(Mutex);
  return 0;
  // CHECK: {{.*Real-time violation.*pthread_mutex_lock.*}}
  // CHECK: {{.*violationLock*}}
  // CHECK-NOT: {{.*pthread_mutex_unlock.*}}
  // CHECK-NOT: {{.*bypassedUnlock.*}}
  // CHECK-NOT: {{.*bypassedLock.*}}
}
