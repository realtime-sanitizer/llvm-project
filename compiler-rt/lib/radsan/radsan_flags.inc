//===-- msan_flags.inc ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// RadSan runtime flags.
//
//===----------------------------------------------------------------------===//
#ifndef RADSAN_FLAG
# error "Define RADSAN_FLAG prior to including this file!"
#endif

// RADSAN_FLAG(Type, Name, DefaultValue, Description)
// See COMMON_FLAG in sanitizer_flags.inc for more details.
