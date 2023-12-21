/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan.h"

#include "radsan_context.h"
#include "radsan_flags.h"
#include "radsan_interceptors.h"

#include "sanitizer_common/sanitizer_atomic.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_flag_parser.h"
#include "sanitizer_common/sanitizer_flags.h"
#include "sanitizer_common/sanitizer_interface_internal.h"
#include "sanitizer_common/sanitizer_libc.h"
#include "sanitizer_common/sanitizer_procmaps.h"
#include "sanitizer_common/sanitizer_stackdepot.h"
#include "sanitizer_common/sanitizer_stacktrace.h"
#include "sanitizer_common/sanitizer_symbolizer.h"

using namespace __sanitizer;

namespace radsan {
bool radsan_inited{};
bool radsan_init_is_running{};
__sanitizer::atomic_uint64_t radsan_report_count{};
static Flags radsan_flags;

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

void radsan_init() {
  using namespace radsan;

  CHECK(!radsan_init_is_running);
  if (radsan_inited) return;
  radsan_init_is_running = true;

  SanitizerToolName = "RealtimeSanitizer";

  initializeFlags();
  initialiseInterceptors(); 

  radsan_inited = true;
  radsan_init_is_running = false;
}

} // namespace radsan

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
