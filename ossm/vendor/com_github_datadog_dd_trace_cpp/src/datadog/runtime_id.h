#pragma once

// This component provides a `class`, `RuntimeID`, that is a wrapper around an
// RFC 4122 UUIDv4. `RuntimeID` identifies the current run of the application in
// which this tracing library is embedded.
//
// See `TracerConfig::runtime_id`, declared in `tracer_config.h`.

#include <string>

namespace datadog {
namespace tracing {

class RuntimeID {
  std::string uuid_;
  RuntimeID();

 public:
  // Return the canonical textual representation of this runtime ID.
  const std::string& string() const { return uuid_; }

  // Return a pseudo-randomly generated runtime ID. The underlying generator is
  // `random_uint64()`, declared in `random.h`.
  static RuntimeID generate();
};

}  // namespace tracing
}  // namespace datadog
