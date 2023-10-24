#pragma once

#define RADSAN_EXPORT __attribute__((visibility("default")))

extern "C" {
RADSAN_EXPORT void radsan_init();
RADSAN_EXPORT void radsan_realtime_enter();
RADSAN_EXPORT void radsan_realtime_exit();
}
