// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <pthread.h>
#include <stdlib.h>

[[clang::realtime_bypass]] void bypassedLock(pthread_mutex_t& Mutex) {
  pthread_mutex_lock(&Mutex);
}

void violationUnlock(pthread_mutex_t& Mutex) {
    pthread_mutex_unlock(&Mutex);
}

[[clang::realtime]] void process(pthread_mutex_t& Mutex) {
  bypassedLock(Mutex);
  violationUnlock(Mutex);
}

int main() {
  pthread_mutex_t Mutex;
  pthread_mutex_init(&Mutex, NULL);

  process(Mutex);
  return 0;
  // CHECK: {{.*Real-time violation.*pthread_mutex_unlock.*}}
  // CHECK-NOT: {{.*pthread_mutex_lock.*}}
}
