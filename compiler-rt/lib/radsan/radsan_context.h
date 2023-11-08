#pragma once

#include <pthread.h>
#include <memory>

namespace radsan {

/*
enum class OnErrorAction {
    Continue,
    LogToFile,
    ExitWithFailure,
};

class IUserInterface {
public:
    virtual OnErrorAction getAction() = 0;
};
*/

class Context {
public:
    void realtimePush();
    void realtimePop();

    void bypassPush();
    void bypassPop();

    void expectNotRealtime(const char *interpreted_function_name);

private:
    bool inRealtimeContext() const;
    bool isBypassed() const;
    void printDiagnostics(const char * intercepted_function_name);

    int realtime_depth_{0};
    int bypass_depth_{0};
    //std::unique_ptr<IUserInterface> user_;
};

Context &getContextForThisThread();

}
