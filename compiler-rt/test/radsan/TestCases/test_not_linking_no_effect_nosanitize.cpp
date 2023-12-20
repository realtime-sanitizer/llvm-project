// RUN: %clangxx %s -o %t
// RUN: %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

#include <stdio.h>
#include <stdlib.h>

__attribute__((no_sanitize("realtime")))
void noSanitizeFree(void* Ptr) {
    free(Ptr);
}

[[clang::realtime]] void violation() {
    void* Ptr = malloc(2);
    noSanitizeFree(Ptr);
}

int main() {
  printf("Starting run\n");

  // Check everything is OK in a realtime block
  violation();

  // Check everything is OK in a non-realtime block
  void* Ptr = malloc(2);
  noSanitizeFree(Ptr);

  printf("No violations ended the program\n");
  return 0;
  // CHECK: {{.*Starting run.*}}
  // CHECK NOT: {{.*Real-time violation.*}}
  // CHECK NOT: {{.*malloc*}}
  // CHECK: {{.*No violations ended the program.*}}
}
