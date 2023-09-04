#pragma once

#include <pthread.h>

namespace radsan {

class Context {
public:
    void realtimePush();
    void realtimePop();

    void abortIfRealtime(const char *interpreted_function_name);

private:
    bool inRealtimeContext() const;
    bool notAlreadyAborting() const;
    void initiateAbort();
    void printDiagnostics(const char * intercepted_function_name);

    int realtime_depth_{0};
    bool already_aborting_{false};
};

Context &getContextForThisThread();

}