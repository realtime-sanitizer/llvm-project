// RUN: %clangxx -fsanitize=realtime %s -o %t
// RUN: not env %run %t 2>&1 | FileCheck %s
// UNSUPPORTED: ios

// Intent: Ensure that Continue mode does not exit on the first violation.

// FIXME: We should have the second "RUN" command be prefaced with "not", 
// aka "not env RADSAN_ERROR_MODE=continue %run %t 2>&1 | FileCheck %s"
// but running in continue mode does not exit non-zero
// https://trello.com/c/vNaKEFty/66-running-in-mode-continue-does-not-exit-non-zero-to-indicate-error
// This test doesn't even currently support "continue mode" because of the c fiasco

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
