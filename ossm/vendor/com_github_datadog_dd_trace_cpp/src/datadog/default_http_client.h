#pragma once

// This component defines a function, `default_http_client`, that returns either
// a `Curl` instance or `nullptr` depending on whether libcurl was included in
// the build.
//
// `default_http_client` is implemented in either `default_http_client_curl.cpp`
// or `default_http_client_null.cpp`.

#include <memory>

#include "clock.h"

namespace datadog {
namespace tracing {

class HTTPClient;
class Logger;

std::shared_ptr<HTTPClient> default_http_client(
    const std::shared_ptr<Logger>& logger, const Clock& clock);

}  // namespace tracing
}  // namespace datadog
