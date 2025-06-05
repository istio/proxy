#pragma once

// This component provides a `struct`, `SpanMatcher`, that is used as a base
// class for `TraceSamplerConfig::Rule`, `SpanSamplerConfig::Rule`, and related
// types. A `SpanMatcher` is a pattern that a given span either matches or
// doesn't, depending on that span's service, operation name, resource name, and
// tags. The member function `SpanMatcher::match` returns whether a specified
// span matches the pattern.
//
// `SpanMatcher` is composed of glob patterns. See `glob.h`.

#include <string>
#include <unordered_map>

#include "expected.h"
#include "json_fwd.hpp"

namespace datadog {
namespace tracing {

struct SpanData;

struct SpanMatcher {
  std::string service = "*";
  std::string name = "*";
  std::string resource = "*";
  // For each (key, value), the tag's key must be present and match literally
  // (no globbing), while the tag's value must match the glob pattern.
  std::unordered_map<std::string, std::string> tags;

  bool match(const SpanData&) const;
  nlohmann::json to_json() const;

  static Expected<SpanMatcher> from_json(const nlohmann::json&);

  bool operator==(const SpanMatcher& other) const {
    return (service == other.service && name == other.name &&
            resource == other.resource && tags == other.tags);
  }
};

static const SpanMatcher catch_all;

}  // namespace tracing
}  // namespace datadog
