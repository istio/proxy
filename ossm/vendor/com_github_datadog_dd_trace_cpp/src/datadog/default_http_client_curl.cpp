#include "curl.h"
#include "default_http_client.h"

// This file is included in the build when libcurl is included in the build.
// It provides an implementation of `default_http_client` that returns a
// `Curl` instance.
//
// If libcurl is not included in the build, then `default_http_client_null.cpp`
// will be built instead.

namespace datadog {
namespace tracing {

std::shared_ptr<HTTPClient> default_http_client(
    const std::shared_ptr<Logger>& logger, const Clock& clock) {
  return std::make_shared<Curl>(logger, clock);
}

}  // namespace tracing
}  // namespace datadog
