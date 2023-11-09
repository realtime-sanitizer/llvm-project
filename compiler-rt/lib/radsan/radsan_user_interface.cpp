#include <radsan/radsan.h>
#include <radsan/radsan_user_interface.h>

#include <sanitizer_common/sanitizer_common.h>

#include <cctype>
#include <cstring>
#include <iostream>

namespace radsan {

std::function<OnErrorAction()> createErrorActionGetter() {
  auto const continue_getter = []() { return OnErrorAction::Continue; };
  auto const exit_getter = []() { return OnErrorAction::ExitWithFailure; };
  auto const interactive_getter = []() {
    auto response = char{};

    std::cout << "Continue? (Y/n): ";
    std::cin >> std::noskipws >> response;

    if (std::toupper(response) == 'N')
      return OnErrorAction::ExitWithFailure;
    else
      return OnErrorAction::Continue;
  };

  auto user_mode = __sanitizer::GetEnv("RADSAN_ERROR_MODE");
  if (user_mode == nullptr) {
    return exit_getter;
  }

  if (std::strcmp(user_mode, "interactive") == 0) {
    return interactive_getter;
  }

  if (std::strcmp(user_mode, "continue") == 0) {
    return continue_getter;
  }

  if (std::strcmp(user_mode, "exit") == 0) {
    return exit_getter;
  }

  return exit_getter;
}

} // namespace radsan
