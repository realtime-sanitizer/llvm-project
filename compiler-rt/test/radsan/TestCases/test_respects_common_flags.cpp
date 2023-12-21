// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: env RADSAN_OPTIONS="exitcode=0" %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

// Even though there is a violation in this run, we set the exitcode to 0 on line 2.
// If this test passes, it means we are playing nice with the established common flags.
// notice the lack of "not" in the RUN command on line 2, which means we expect the process to exit with 0.

#include <stdlib.h>

void* mallocViolation() {
  return malloc(10);
}

[[clang::realtime]] void process() {
  void* Ptr = mallocViolation();
}

int main() {
  process();
  return 0;
  // CHECK: {{.*Real-time violation.*}}
  // CHECK: {{.*malloc*}}
}
