#include <radsan/radsan_user_interface.h>
#include <radsan/radsan.h>

#include <sanitizer_common/sanitizer_common.h>

#include <cctype>
#include <iostream>

namespace radsan {

namespace {
class SingleErrorActionUserInterface : public IUserInterface {
public:
    SingleErrorActionUserInterface (OnErrorAction action)
        : action_ (action)
    {}

    OnErrorAction getAction() override {
        return action_;
    }

private:
    OnErrorAction action_ {};
};

class InteractiveUserInterface : public IUserInterface {
public:
    OnErrorAction getAction() override {
        auto response = char {};

        std::cout << "Continue? (Y/n): ";
        std::cin >> std::noskipws >> response;

        if (std::toupper (response) == 'N')
            return OnErrorAction::ExitWithFailure;
        else
            return OnErrorAction::Continue;
    }
};
}

std::unique_ptr<IUserInterface> createUserInterface() {
    auto user_mode = __sanitizer::GetEnv("RADSAN_ERROR_MODE");

    if (std::strcmp(user_mode, "interactive") == 0) {
        return std::make_unique<InteractiveUserInterface>();
    } else if (std::strcmp(user_mode, "continue") == 0) {
        return std::make_unique<SingleErrorActionUserInterface>(
            OnErrorAction::Continue);
    } else if (std::strcmp(user_mode, "exit") == 0) {
        return std::make_unique<SingleErrorActionUserInterface>(
            OnErrorAction::ExitWithFailure);
    }

    return std::make_unique<SingleErrorActionUserInterface>(
        OnErrorAction::ExitWithFailure);
}

}
