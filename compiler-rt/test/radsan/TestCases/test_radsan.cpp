// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdlib.h>

[[clang::realtime]] void violation() {
    auto x = malloc(2);
}

int main() {
  violation();
  return 0;
  // CHECK: {{.*Real-time violation.*}}
  // CHECK: {{.*malloc*}}
}
