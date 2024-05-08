/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#pragma once

namespace radsan {

class Context {
public:
  Context();

  void RealtimePush();
  void RealtimePop();

  void BypassPush();
  void BypassPop();

  void ExpectNotRealtime(const char *InterceptedFunctionName);

private:
  bool InRealtimeContext() const;
  bool IsBypassed() const;
  void PrintDiagnostics(const char *InterceptedFunctionName);

  int RealtimeDepth{0};
  int BypassDepth{0};
};

Context &GetContextForThisThread();

} // namespace radsan
