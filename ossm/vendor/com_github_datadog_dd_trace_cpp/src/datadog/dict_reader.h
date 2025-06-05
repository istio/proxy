#pragma once

// This component provides an interface, `DictReader`, that represents a
// read-only key/value mapping of strings.  It's used when extracting trace
// context from externalized formats: HTTP headers, gRPC metadata, etc.

#include <functional>

#include "optional.h"
#include "string_view.h"

namespace datadog {
namespace tracing {

class DictReader {
 public:
  virtual ~DictReader() {}

  // Return the value at the specified `key`, or return `nullopt` if there
  // is no value at `key`.
  virtual Optional<StringView> lookup(StringView key) const = 0;

  // Invoke the specified `visitor` once for each key/value pair in this object.
  virtual void visit(
      const std::function<void(StringView key, StringView value)>& visitor)
      const = 0;
};

}  // namespace tracing
}  // namespace datadog
