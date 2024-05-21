/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#pragma once

#include "sanitizer_common/sanitizer_internal_defs.h"

extern "C" {

// Initialise radsan interceptors.
// A call to this method is added to the preinit array on Linux systems.
SANITIZER_INTERFACE_ATTRIBUTE void __radsan_init();

// Enter real-time context.
// When in a real-time context, RADSan interceptors will error if realtime
// violations are detected. Calls to this method are injected at the code
// generation stage when RADSan is enabled.
SANITIZER_INTERFACE_ATTRIBUTE void __radsan_realtime_enter();

// Exit the real-time context.
// When not in a real-time context, RADSan interceptors will simply forward
// intercepted method calls to the real methods.
SANITIZER_INTERFACE_ATTRIBUTE void __radsan_realtime_exit();

// Disable all RADSan error reporting.
// Injected into the code if "nosanitize(realtime)" is on a function.
SANITIZER_INTERFACE_ATTRIBUTE void __radsan_off();

// Re-enable all RADSan error reporting.
// The counterpart to `__radsan_off`.
SANITIZER_INTERFACE_ATTRIBUTE void __radsan_on();

} // extern "C"
