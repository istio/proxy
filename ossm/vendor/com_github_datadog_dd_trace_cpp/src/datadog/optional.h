#pragma once

// One of the clients of this library is Envoy, a service (HTTP) proxy.
//
// Envoy uses Abseil as its base C++ library, and additionally builds in C++17
// mode.  Abseil has a build option to forward its `std::string_view` and
// `std::optional` equivalents to the actual standard types when C++17 is
// available.
//
// Envoy does not use this Abseil build option, due to incomplete support for
// the C++17 standard library on iOS 11.
//
// As a result, Envoy forbids use of `std::string_view` and `std::optional`,
// instead preferring Abseil's `absl::string_view` and `absl::optional`.
//
// This presents a problem for this library, since we use `std::string_view`
// and `std::optional` in the exported interface, i.e. in header files.
//
// As a workaround, Bazel (the build tool used by Envoy) builds of this library
// will define the `DD_USE_ABSEIL_FOR_ENVOY` preprocessor macro.  When this
// macro is defined, the library-specific `StringView` and `Optional` aliases
// will refer to the Abseil types.  When the macro is not defined, the
// library-specific aliases will refer to the standard types.
//
// This file defines `datadog::tracing::Optional`, a type template that is an
// alias for either `std::optional` or `absl::optional`.

#ifdef DD_USE_ABSEIL_FOR_ENVOY
// Abseil examples, including usage in Envoy, include Abseil headers in quoted
// style instead of angle bracket style, per Bazel's default build behavior.
#include "absl/types/optional.h"
#else
#include <optional>
#endif  // defined DD_USE_ABSEIL_FOR_ENVOY

#include <utility>  // std::forward

namespace datadog {
namespace tracing {

#ifdef DD_USE_ABSEIL_FOR_ENVOY
template <typename Value>
using Optional = absl::optional<Value>;
inline constexpr auto nullopt = absl::nullopt;
#else
template <typename Value>
using Optional = std::optional<Value>;
inline constexpr auto nullopt = std::nullopt;
#endif  // defined DD_USE_ABSEIL_FOR_ENVOY

// Return the first non-null argument value.  The last argument must not be
// `Optional`.
template <typename Value>
auto value_or(Value&& value) {
  return std::forward<Value>(value);
}

template <typename Value, typename... Rest>
auto value_or(Optional<Value> maybe, Rest&&... rest) {
  return maybe.value_or(value_or(std::forward<Rest>(rest)...));
}

}  // namespace tracing
}  // namespace datadog
