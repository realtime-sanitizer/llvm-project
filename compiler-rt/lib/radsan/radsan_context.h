#pragma once

#include <radsan/radsan_user_interface.h>

#include <functional>

namespace radsan {

class Context {
public:
  Context();
  Context(std::function<OnErrorAction()> get_error_action);

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
  std::function<OnErrorAction()> get_error_action_;
};

Context &getContextForThisThread();

} // namespace radsan
