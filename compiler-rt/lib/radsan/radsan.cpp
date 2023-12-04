/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include <radsan/radsan.h>
#include <radsan/radsan_context.h>
#include <radsan/radsan_interceptors.h>
#include <unistd.h>

extern "C" {
RADSAN_EXPORT void radsan_init() { radsan::initialiseInterceptors(); }

RADSAN_EXPORT void radsan_realtime_enter() {
  radsan::getContextForThisThread().realtimePush();
}

RADSAN_EXPORT void radsan_realtime_exit() {
  radsan::getContextForThisThread().realtimePop();
}

RADSAN_EXPORT void radsan_off() {
  radsan::getContextForThisThread().bypassPush();
}

RADSAN_EXPORT void radsan_on() {
  radsan::getContextForThisThread().bypassPop();
}
}
