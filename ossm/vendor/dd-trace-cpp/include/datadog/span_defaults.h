#pragma once

// This component provides a `struct`, `SpanDefaults`, used to fill out `Span`
// properties that are not otherwise specified in a `SpanConfig`.
// `SpanDefaults` are specified as the `defaults` property of `TracerConfig`.

#include <string>
#include <unordered_map>

namespace datadog {
namespace tracing {

struct SpanDefaults {
  std::string service;
  std::string service_type = "web";
  std::string environment = "";
  std::string version = "";
  std::string name = "";
  std::unordered_map<std::string, std::string> tags;
};

inline bool operator==(const SpanDefaults& lhs, const SpanDefaults& rhs) {
#define EQ(FIELD) lhs.FIELD == rhs.FIELD
  return EQ(service) && EQ(service_type) && EQ(environment) && EQ(version) &&
         EQ(name) && EQ(tags);
#undef EQ
}

}  // namespace tracing
}  // namespace datadog
