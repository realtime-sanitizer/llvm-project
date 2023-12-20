// RUN: %clangxx %s -o %t
// RUN: %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdlib.h>
#include <stdio.h>

// In this test, we don't use the -fsanitize=realtime flag, so nothing
// should happen here
[[clang::realtime]] void violation() {
    void* x = malloc(2);
}

int main() {
  printf("Starting run\n");
  violation();
  printf("No violations ended the program\n");
  return 0;
  // CHECK: {{.*Starting run.*}}
  // CHECK NOT: {{.*Real-time violation.*}}
  // CHECK NOT: {{.*malloc*}}
  // CHECK: {{.*No violations ended the program.*}}
}
