// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

// Intent: Ensure that a no_sanitize attribute has no impact 
//         if not in a [[clang::realtime]] function

#include <stdio.h>
#include <stdlib.h>

__attribute__((no_sanitize("realtime")))
void violation() {
    void* Ptr = malloc(2);
    free(Ptr);
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
