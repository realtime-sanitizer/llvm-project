// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

// Intent: Ensure that an intercepted call in a [[clang::realtime]] function
//         is flagged as an error. Basic smoke test.

#include <stdlib.h>

[[clang::realtime]] void violation() {
    void* Ptr = malloc(2);
}

int main() {
  violation();
  return 0;
  // CHECK: {{.*Real-time violation.*}}
  // CHECK: {{.*malloc*}}
}
