#pragma once

// This component provides an interface, `DictWriter`, that represents a
// write-only key/value mapping of strings.  It's used when injecting trace
// context into externalized formats: HTTP headers, gRPC metadata, etc.
//
// Note that while the data structure modeled is a mapping, duplicate keys are
// permitted to result from repeated invocations of `DictWriter::set` with the
// same key.

#include "string_view.h"

namespace datadog {
namespace tracing {

class DictWriter {
 public:
  virtual ~DictWriter() {}

  // Associate the specified `value` with the specified `key`.  An
  // implementation may, but is not required to, overwrite any previous value at
  // `key`.
  virtual void set(StringView key, StringView value) = 0;
};

}  // namespace tracing
}  // namespace datadog
