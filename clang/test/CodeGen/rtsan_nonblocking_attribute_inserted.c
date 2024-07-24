// RUN: %clang     -target x86_64-unknown-linux -fsanitize=realtime %s -S -emit-llvm -o - | FileCheck %s

float process(float *a) [[clang::nonblocking]] { return *a; }

// CHECK: define{{.*}}process{{.*}}#0 {
// CHECK: attributes #0 = {
// CHECK-SAME: {{.*nonblocking.*}}
