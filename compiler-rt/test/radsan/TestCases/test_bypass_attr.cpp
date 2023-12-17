// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <pthread.h>
#include <stdlib.h>

[[clang::realtime_bypass]] void* bypassedMalloc() {
  void* Ptr = malloc(2);
  return Ptr;
}

[[clang::realtime_bypass]]void bypassedFree(void* Input) {
  free(Input);
}

void* violationMalloc() {
  void* Ptr = malloc(2);
  return Ptr;
}

[[clang::realtime]] void process() {
  void* Ptr = bypassedMalloc();
  bypassedFree(Ptr);

  void* Ptr2 = violationMalloc();
  bypassedFree(Ptr2);
}

int main() {
  process();
  return 0;
  // CHECK: {{.*Real-time violation.*malloc.*}}
  // CHECK: {{.*violationMalloc*}}
  // CHECK-NOT: {{.*free.*}}
  // CHECK-NOT: {{.*bypassedFree.*}}
  // CHECK-NOT: {{.*bypassedMalloc.*}}
}
