#pragma once

#include <pthread.h>

namespace radsan {

class Context {
public:
    void realtimePush();
    void realtimePop();

    void bypassPush();
    void bypassPop();

    void exitIfRealtime(const char *interpreted_function_name);

private:
    bool inRealtimeContext() const;
    bool alreadyExiting() const;
    bool isBypassed() const;
    void initiateExit();
    void printDiagnostics(const char * intercepted_function_name);

    int realtime_depth_{0};
    int bypass_depth_{0};
    bool already_exiting_{false};
};

Context &getContextForThisThread();

}
