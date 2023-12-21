/**
    This file is part of the RealtimeSanitizer (RADSan) project.
    https://github.com/realtime-sanitizer/radsan

    Copyright 2023 David Trevelyan & Alistair Barker
    Subject to GNU General Public License (GPL) v3.0
*/

#include <radsan/radsan.h>
#include <radsan/radsan_user_interface.h>

#include <sanitizer_common/sanitizer_common.h>

#include <cctype>
#include <cstring>
#include <iostream>

namespace radsan {

std::function<OnErrorAction()> createErrorActionGetter() {
  ENSURE_RADSAN_INITED();
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

  const char* user_mode = radsan::flags()->error_mode;

  if (std::strcmp(user_mode, "interactive") == 0) {
    return interactive_getter;
  }
  else if (std::strcmp(user_mode, "continue") == 0) {
    return continue_getter;
  }
  else if (std::strcmp(user_mode, "exit") == 0) {
    return exit_getter;
  }

  __sanitizer::Printf("WARNING Invalid error mode: %s. Assuming 'exit'\n", user_mode);

  return exit_getter;
}

} // namespace radsan
