// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdlib.h>
#include <pthread.h>

[[clang::realtime_bypass]] void bypassed_lock(pthread_mutex_t& m) {
  pthread_mutex_lock(&m);
}

void violation_unlock(pthread_mutex_t& m) {
    pthread_mutex_unlock(&m);
}

[[clang::realtime]] void process(pthread_mutex_t& m) {
  bypassed_lock(m);
  violation_unlock(m);
}

int main() {
  pthread_mutex_t m;
  pthread_mutex_init(&m, NULL);

  process(m);
  return 0;
  // CHECK: {{.*Real-time violation.*pthread_mutex_unlock.*}}
  // CHECK-NOT: {{.*pthread_mutex_lock.*}}
}
