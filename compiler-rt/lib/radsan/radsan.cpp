/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include <radsan/radsan.h>
#include <radsan/radsan_context.h>
#include <radsan/radsan_interceptors.h>

extern "C" {

SANITIZER_INTERFACE_ATTRIBUTE void radsan_init() {
  __radsan::InitializeInterceptors();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_realtime_enter() {
  __radsan::GetContextForThisThread().RealtimePush();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_realtime_exit() {
  __radsan::GetContextForThisThread().RealtimePop();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_off() {
  __radsan::GetContextForThisThread().BypassPush();
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_on() {
  __radsan::GetContextForThisThread().BypassPop();
}

} // extern "C"
