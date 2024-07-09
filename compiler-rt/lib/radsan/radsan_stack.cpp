/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include "radsan_stack.h"

#include <sanitizer_common/sanitizer_flags.h>
#include <sanitizer_common/sanitizer_stacktrace.h>

using namespace __sanitizer;

// We must define our own implementation of this method for our runtime.
// This one is just copied from UBSan.

namespace __sanitizer {
void BufferedStackTrace::UnwindImpl(uptr pc, uptr bp, void *context,
                                    bool request_fast, u32 max_depth) {
  uptr top = 0;
  uptr bottom = 0;
  GetThreadStackTopAndBottom(false, &top, &bottom);
  bool fast = StackTrace::WillUseFastUnwind(request_fast);
  Unwind(max_depth, pc, bp, context, top, bottom, fast);
}
} // namespace __sanitizer

namespace __radsan {
void SetGlobalStackTraceFormat() {
  SetCommonFlagsDefaults();
  CommonFlags cf;
  cf.CopyFrom(*common_flags());
  cf.stack_trace_format = "DEFAULT";
  cf.external_symbolizer_path = GetEnv("RADSAN_SYMBOLIZER_PATH");
  OverrideCommonFlags(cf);
}
} // namespace __radsan

using namespace __radsan;
void __radsan::PrintStackTrace() {

  BufferedStackTrace Stack{};

  GET_CURRENT_PC_BP;
  Stack.Unwind(pc, bp, nullptr, common_flags()->fast_unwind_on_fatal);

  SetGlobalStackTraceFormat();
  Stack.Print();
}
