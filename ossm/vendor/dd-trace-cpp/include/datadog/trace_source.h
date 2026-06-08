#pragma once

#include <datadog/string_view.h>

namespace datadog {
namespace tracing {

/// Enumerates the possible trace sources that can generate a span.
///
/// This enum class identifies the different products that can create a span.
/// Each source is represented by a distinct bit flag, allowing for bitwise
/// operations.
enum class Source : char {
  apm = 0x01,
  appsec = 0x02,
  datastream_monitoring = 0x04,
  datajob_monitoring = 0x08,
  database_monitoring = 0x10,
};

/// Validates if a given string corresponds to a valid trace source.
///
/// This function checks whether the provided string matches any of the
/// predefined trace sources specified in the Source enum. It is useful for
/// ensuring that a source string obtained from an external input is valid
/// before further processing.
///
/// @param source_str A string view representing the trace source to validate.
///
/// @return true if the source string is valid and corresponds to a known trace
/// source, false otherwise.
bool validate_trace_source(StringView source_str);

/// Converts a Source enum value to its corresponding string representation
inline constexpr StringView to_tag(Source source) {
  switch (source) {
    case Source::apm:
      return "01";
    case Source::appsec:
      return "02";
    case Source::database_monitoring:
      return "04";
    case Source::datajob_monitoring:
      return "08";
    case Source::datastream_monitoring:
      return "10";
  }

  return "";
}

}  // namespace tracing
}  // namespace datadog
