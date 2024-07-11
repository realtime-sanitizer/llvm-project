/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include <radsan/radsan.h>

#include <radsan/radsan_context.h>
#include <radsan/radsan_flags.h>
#include <radsan/radsan_interceptors.h>

#include "sanitizer_common/sanitizer_atomic.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_flag_parser.h"
#include "sanitizer_common/sanitizer_flags.h"

using namespace __sanitizer;

namespace radsan {
static __sanitizer::atomic_uint8_t radsan_inited{};
static Mutex radsan_init_mutex{};
static Flags radsan_flags{};

Flags *flags() { return &radsan_flags; }


SANITIZER_INTERFACE_WEAK_DEF(const char *, __radsan_default_options, void) {
  return "";
}


static void RegisterRadsanFlags(FlagParser *parser, Flags *f) {
#define RADSAN_FLAG(Type, Name, DefaultValue, Description) \
  RegisterFlag(parser, #Name, Description, &f->Name);
#include "radsan_flags.inc"
#undef RADSAN_FLAG
}

void Flags::SetDefaults() {
#define RADSAN_FLAG(Type, Name, DefaultValue, Description) Name = DefaultValue;
#include "radsan_flags.inc"
#undef RADSAN_FLAG
}


static void initializeFlags() {
  SetCommonFlagsDefaults();
  {
    CommonFlags cf;
    cf.CopyFrom(*common_flags());
    cf.stack_trace_format = "DEFAULT";
    cf.external_symbolizer_path = GetEnv("RADSAN_SYMBOLIZER_PATH");
    OverrideCommonFlags(cf);
  }

  Flags *f = flags();
  f->SetDefaults();

  FlagParser parser;
  RegisterRadsanFlags(&parser, f);
  RegisterCommonFlags(&parser);

  // Override from user-specified string.
  parser.ParseString(__radsan_default_options());

  parser.ParseStringFromEnv("RADSAN_OPTIONS");

  InitializeCommonFlags();

  if (Verbosity()) ReportUnrecognizedFlags();

  if (common_flags()->help) 
  {
    parser.PrintFlagDescriptions();
  }
}

} // namespace radsan

extern "C" {

SANITIZER_INTERFACE_ATTRIBUTE void radsan_ensure_initialized() {
  // Double-checked locking.
  // Ensure that radsan_init() is called only once by the first thread
  // that gets here.

  if (radsan_is_initialized()) 
    return;

  Lock lock(&radsan::radsan_init_mutex);

  if (radsan_is_initialized()) 
    return;

  radsan_init();
}

SANITIZER_INTERFACE_ATTRIBUTE bool radsan_is_initialized() {
  return __sanitizer::atomic_load(&radsan::radsan_inited, memory_order_acquire) == 1; 
}

SANITIZER_INTERFACE_ATTRIBUTE void radsan_init() {
  using namespace radsan;

  if (radsan_is_initialized()) return;

  SanitizerToolName = "RealtimeSanitizer";

  initializeFlags();
  initialiseInterceptors(); 

  __sanitizer::atomic_store(&radsan_inited, 1, memory_order_release);
}

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
