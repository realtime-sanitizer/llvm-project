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

namespace __radsan {

void __radsan_init() {
  radsan::initialiseInterceptors(); 
}

} // namespace __radsan

extern "C" {

SANITIZER_INTERFACE_ATTRIBUTE void radsan_realtime_enter() {
  radsan::getContextForThisThread().realtimePush();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_realtime_exit() {
  radsan::getContextForThisThread().realtimePop();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_off() {
  radsan::getContextForThisThread().bypassPush();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_on() {
  radsan::getContextForThisThread().bypassPop();
}
}
