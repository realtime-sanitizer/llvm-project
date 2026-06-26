//===--- rtsan_optional.h - Realtime Sanitizer ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#pragma once

namespace __rtsan {

struct Nullopt {};

// Minimal Optional<T> for use in the RTSan runtime, which cannot depend
// on LLVM's ADT or C++23's std::optional. Intentionally restricted to small,
// trivially-copyable value types. Construction takes T by value, and no
// forwarding is provided. Post-construction mutation is likewise omitted.
// Extend as needed.
template <typename T> class Optional {
public:
  constexpr Optional() noexcept;
  constexpr Optional(T value) noexcept;
  constexpr Optional(Nullopt) noexcept;

  constexpr bool HasValue() const noexcept;
  constexpr T const &Value() const noexcept;

private:
  enum class Status { Value, NoValue } status_;
  T value_;
};

// Implementation details...

template <typename T>
constexpr Optional<T>::Optional() noexcept : Optional(Nullopt{}) {}

template <typename T>
constexpr Optional<T>::Optional(T value) noexcept
    : status_(Status::Value), value_(value) {}

template <typename T>
constexpr Optional<T>::Optional(Nullopt) noexcept : status_(Status::NoValue) {}

template <typename T> constexpr bool Optional<T>::HasValue() const noexcept {
  return status_ == Status::Value;
}

template <typename T> constexpr T const &Optional<T>::Value() const noexcept {
  return value_;
}

} // namespace __rtsan
