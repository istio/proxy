#pragma once

#define CATCH_CONFIG_CPP17_UNCAUGHT_EXCEPTIONS
#define CATCH_CONFIG_CPP17_STRING_VIEW
#define CATCH_CONFIG_CPP17_VARIANT
#define CATCH_CONFIG_CPP17_OPTIONAL
#define CATCH_CONFIG_CPP17_BYTE

#include <datadog/expected.h>
#include <datadog/optional.h>
#include <datadog/string_view.h>
#include <datadog/trace_id.h>

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "catch.hpp"

namespace std {

std::ostream& operator<<(std::ostream& stream,
                         const std::pair<const std::string, std::string>& item);

std::ostream& operator<<(std::ostream& stream,
                         const std::optional<std::string_view>& maybe);

std::ostream& operator<<(std::ostream& stream, const std::optional<int>& maybe);

}  // namespace std

namespace datadog {
namespace tracing {

template <typename Value>
std::ostream& operator<<(std::ostream& stream,
                         const Expected<Value>& expected) {
  if (expected) {
    return stream << "?";  // don't know in general
  }
  return stream << expected.error();
}

std::ostream& operator<<(std::ostream&, TraceID);

}  // namespace tracing
}  // namespace datadog
