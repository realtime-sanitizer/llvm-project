// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdlib.h>
#include <pthread.h>

[[clang::realtime_bypass]] void* bypassed_malloc() {
  void* x = malloc(2);
  return x;
}

void violation_free(void* input) {
  free(input);
}

[[clang::realtime]] void process() {
  void* x = bypassed_malloc();
  violation_free(x);
}

int main() {
  process();
  return 0;
  // CHECK: {{.*Real-time violation.*free.*}}
  // CHECK-NOT: {{.*malloc.*}}
}
