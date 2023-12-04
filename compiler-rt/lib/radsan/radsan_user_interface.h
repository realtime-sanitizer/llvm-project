#pragma once

#include <functional>

namespace radsan {

enum class OnErrorAction {
    Continue,
    ExitWithFailure,
};

std::function<OnErrorAction()> createErrorActionGetter();

}
