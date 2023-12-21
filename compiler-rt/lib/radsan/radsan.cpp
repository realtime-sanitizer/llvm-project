/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan/radsan.h"
#include "radsan/radsan_context.h"
#include "radsan/radsan_interceptors.h"
#include "sanitizer_common/sanitizer_common.h"

#include <unistd.h>
#include <atomic>

namespace radsan {
std::atomic<bool> radsan_inited = false;
std::atomic<bool> radsan_init_is_running = false;
std::atomic<int> radsan_report_count = 0;
} // namespace radsan

void radsan_init() {
  using namespace radsan;

  CHECK(!radsan_init_is_running);
  if (radsan_inited) return;
  radsan_init_is_running = true;

  // SanitizerToolName = "RealtimeSanitizer";


  initialiseInterceptors(); 
}

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
