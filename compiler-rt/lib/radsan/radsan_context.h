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

  void realtimePush();
  void realtimePop();

  void bypassPush();
  void bypassPop();

  void expectNotRealtime(const char *interpreted_function_name);

private:
  bool inRealtimeContext() const;
  bool isBypassed() const;
  void printDiagnostics(const char *intercepted_function_name);

  int realtime_depth_{0};
  int bypass_depth_{0};
};

Context &getContextForThisThread();

} // namespace radsan
