//===-- radsan_flags.h --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of MemorySanitizer.
//
//===----------------------------------------------------------------------===//
#ifndef RADSAN_FLAGS_H
#define RADSAN_FLAGS_H

namespace radsan {

struct Flags {
#define RADSAN_FLAG(Type, Name, DefaultValue, Description) Type Name;
#include "radsan_flags.inc"
#undef RADSAN_FLAG

  void SetDefaults();
};

Flags *flags();

}  // namespace radsan

#endif  // RADSAN_FLAGS_H
