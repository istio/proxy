#pragma once

// This component provides a `struct`, `SpanDefaults`, used to fill out `Span`
// properties that are not otherwise specified in a `SpanConfig`.
// `SpanDefaults` are specified as the `defaults` property of `TracerConfig`.

#include <string>
#include <unordered_map>

#include "json_fwd.hpp"

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

nlohmann::json to_json(const SpanDefaults&);

bool operator==(const SpanDefaults&, const SpanDefaults&);

}  // namespace tracing
}  // namespace datadog
