// RUN: %clangxx %s -o %t
// RUN: %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdlib.h>
#include <stdio.h>

__attribute__((no_sanitize("realtime")))
void no_sanitize_free(void* x) {
    free(x);
}

[[clang::realtime]] void violation() {
    void* x = malloc(2);
    no_sanitize_free(x);
}

int main() {
  printf("Starting run\n");

  // Check everything is OK in a realtime block
  violation();

  // Check everything is OK in a non-realtime block
  void* x = malloc(2);
  no_sanitize_free(x);

  printf("No violations ended the program\n");
  return 0;
  // CHECK: {{.*Starting run.*}}
  // CHECK NOT: {{.*Real-time violation.*}}
  // CHECK NOT: {{.*malloc*}}
  // CHECK: {{.*No violations ended the program.*}}
}
