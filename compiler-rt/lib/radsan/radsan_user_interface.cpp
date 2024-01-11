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

bool ShouldExit() {

  const bool defaultShouldExit = true;

  static const char* user_mode = __sanitizer::GetEnv("RADSAN_ERROR_MODE");
  if (user_mode == nullptr) {
    return defaultShouldExit;
  }

  if (std::strcmp(user_mode, "interactive") == 0) {
    auto response = char{};

    std::cout << "Continue? (Y/n): ";
    std::cin >> std::noskipws >> response;

    return std::toupper(response) == 'N';
  }

  if (std::strcmp(user_mode, "continue") == 0) {
    return false;
  }

  if (std::strcmp(user_mode, "exit") == 0) {
    return true;
  }

  return defaultShouldExit;
}

} // namespace radsan
