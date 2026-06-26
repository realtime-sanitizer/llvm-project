//===--- rtsan_expected.h - Realtime Sanitizer ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#pragma once

#include "rtsan/rtsan_optional.h"

namespace __rtsan {

template <typename E> class Unexpected;

// Minimal Expected<T, E> for use in the RTSan runtime, which cannot depend
// on LLVM's ADT or C++23's std::expected. Intentionally restricted to small,
// trivially-copyable value and error types. Construction takes T and E by
// value, and no forwarding is provided. Post-construction mutation is likewise
// omitted. Extend as needed.
template <typename T, typename E> class Expected {
public:
  constexpr Expected() noexcept;
  constexpr Expected(T value) noexcept;
  constexpr Expected(Unexpected<E> error) noexcept;

  constexpr bool HasValue() const noexcept;
  constexpr T const &Value() const noexcept;
  constexpr E const &Error() const noexcept;

private:
  enum class Status { Value, Error } status_;
  union {
    T value_;
    E error_;
  };
};

template <typename E> class Expected<void, E> {
public:
  constexpr Expected() noexcept;
  constexpr Expected(Unexpected<E> error) noexcept;
  constexpr bool HasValue() const noexcept;
  constexpr E const &Error() const noexcept;

private:
  bool has_value_;
  union {
    E error_;
  };
};

template <typename E> class Unexpected {
public:
  constexpr explicit Unexpected(E e) noexcept;
  constexpr E const &Error() const noexcept;

private:
  E error_{};
};

template <typename E> Unexpected(E) -> Unexpected<E>;

// Implementation details...

template <typename T, typename E>
constexpr Expected<T, E>::Expected() noexcept : Expected(T{}) {}

template <typename E>
constexpr Expected<void, E>::Expected() noexcept : has_value_(true) {}

template <typename T, typename E>
constexpr Expected<T, E>::Expected(T value) noexcept
    : status_(Status::Value), value_(value) {}

template <typename T, typename E>
constexpr Expected<T, E>::Expected(Unexpected<E> error) noexcept
    : status_(Status::Error), error_(error.Error()) {}

template <typename E>
constexpr Expected<void, E>::Expected(Unexpected<E> error) noexcept
    : has_value_(false), error_(error.Error()) {}

template <typename T, typename E>
constexpr bool Expected<T, E>::HasValue() const noexcept {
  return status_ == Status::Value;
}

template <typename E>
constexpr bool Expected<void, E>::HasValue() const noexcept {
  return has_value_;
}

template <typename T, typename E>
constexpr T const &Expected<T, E>::Value() const noexcept {
  return value_;
}

template <typename T, typename E>
constexpr E const &Expected<T, E>::Error() const noexcept {
  return error_;
}

template <typename E>
constexpr E const &Expected<void, E>::Error() const noexcept {
  return error_;
}

template <typename E>
constexpr Unexpected<E>::Unexpected(E e) noexcept : error_(e) {}

template <typename E> constexpr E const &Unexpected<E>::Error() const noexcept {
  return error_;
}

} // namespace __rtsan
