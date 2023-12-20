// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: %env_radsan_opts=continue not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdlib.h>

void* mallocViolation() {
  return malloc(10);
}

void freeViolation(void* Ptr) {
  free(Ptr);
}

[[clang::realtime]] void process() {
  void* Ptr = mallocViolation();
  freeViolation(Ptr);
}

int main() {
  process();
  return 0;
  // CHECK: {{.*Real-time violation.*}}
  // CHECK: {{.*malloc*}}
}
