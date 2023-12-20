/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan.h"

#if SANITIZER_CAN_USE_PREINIT_ARRAY

// The symbol is called __local_radsan_preinit, because it's not intended to be
// exported.
// This code is linked into the main executable when -fsanitize=realtime is in
// the link flags. It can only use exported interface functions.
__attribute__((section(".preinit_array"), used))
void (*__local_radsan_preinit)(void) = __radsan_init;

#endif
