#include "sanitizer_common/sanitizer_internal_defs.h"
#include <radsan/radsan.h>

#if SANITIZER_CAN_USE_PREINIT_ARRAY

// The symbol is called __local_radsan_preinit, because it's not intended to be
// exported.
// This code linked into the main executable when -fsanitize=realtime is in
// the link flags. It can only use exported interface functions.
__attribute__((section(".preinit_array"), used))
void (*__local_radsan_preinit)(void) = radsan_init;

#endif
