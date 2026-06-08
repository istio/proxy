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
// This file defines `datadog::tracing::StringView`, a type that is an alias
// for either `std::string_view` or `absl::string_view`.

#include <string>

#ifdef DD_USE_ABSEIL_FOR_ENVOY
// Abseil examples, including usage in Envoy, include Abseil headers in quoted
// style instead of angle bracket style, per Bazel's default build behavior.
#include "absl/strings/string_view.h"
#else
#include <string_view>
#endif  // defined DD_USE_ABSEIL_FOR_ENVOY

namespace datadog {
namespace tracing {

#ifdef DD_USE_ABSEIL_FOR_ENVOY
using StringView = absl::string_view;
#else
using StringView = std::string_view;
#endif  // defined DD_USE_ABSEIL_FOR_ENVOY

// When `StringView` is not the same as `std::string_view`,
// `operator+=(string&, StringView)` isn't defined.  To work around this, use
// `append` everywhere.
inline void append(std::string& destination, StringView text) {
  destination.append(text.data(), text.size());
}

// When `StringView` is not the same as `std::string_view`,
// `operator=(string&, StringView)` isn't defined.  To work around this, use
// `assign` everywhere.
inline void assign(std::string& destination, StringView text) {
  destination.assign(text.data(), text.size());
}

inline bool contains(StringView text, StringView pattern) {
  return text.find(pattern) != text.npos;
}

}  // namespace tracing
}  // namespace datadog
