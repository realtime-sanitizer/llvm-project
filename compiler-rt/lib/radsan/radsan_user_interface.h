#pragma once

#include <memory>

namespace radsan {

enum class OnErrorAction {
    Continue,
    ExitWithFailure,
};

class IUserInterface {
public:
    virtual ~IUserInterface() = default;
    virtual OnErrorAction getAction() = 0;

    // TODO add this so we can silence stack trace printouts in tests
    // virtual void display (const char * message) = 0;
};

std::unique_ptr<IUserInterface> createUserInterface();

}
