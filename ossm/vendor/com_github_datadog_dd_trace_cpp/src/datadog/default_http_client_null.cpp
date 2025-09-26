#include "default_http_client.h"

// This file is included in the build when libcurl is not included in the build.
// It provides an implementation of `default_http_client` that returns null,
// which means that a user configuring a tracer with `TracerConfig` must
// either specify a custom `Collector`, or an `HTTPClient` within
// `DatadogAgentConfig`.

namespace datadog {
namespace tracing {

std::shared_ptr<HTTPClient> default_http_client(const std::shared_ptr<Logger> &,
                                                const Clock &) {
  return nullptr;
}

}  // namespace tracing
}  // namespace datadog
