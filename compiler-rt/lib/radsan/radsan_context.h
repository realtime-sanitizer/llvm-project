/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#pragma once

namespace __radsan {

class Context {
public:
  Context();

  void RealtimePush();
  void RealtimePop();

  void BypassPush();
  void BypassPop();

  void ExpectNotRealtime(const char *intercepted_function_name);

private:
  bool InRealtimeContext() const;
  bool IsBypassed() const;
  void PrintDiagnostics(const char *intercepted_function_name);

  int realtime_depth{0};
  int bypass_depth{0};
};

Context &GetContextForThisThread();

} // namespace __radsan
