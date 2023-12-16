// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <pthread.h>
#include <stdlib.h>

[[clang::realtime_bypass]] void* bypassedMalloc() {
  void* Ptr = malloc(2);
  return Ptr;
}

void violationFree(void* Input) {
  free(Input);
}

[[clang::realtime]] void process() {
  void* Ptr = bypassedMalloc();
  violationFree(Ptr);
}

int main() {
  process();
  return 0;
  // CHECK: {{.*Real-time violation.*free.*}}
  // CHECK-NOT: {{.*malloc.*}}
}
