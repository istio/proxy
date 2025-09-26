#pragma once

// This component provides an interface, `HTTPClient`, that represents an
// asynchronous HTTP client.
//
// `HTTPClient` is used by `DatadogAgent` to send traces to the Datadog Agent.
//
// If this library was built with support for libcurl, then `Curl` implements
// `HTTPClient` in terms of libcurl.  See `curl.h`.

#include <chrono>
#include <functional>

#include "error.h"
#include "expected.h"
#include "json_fwd.hpp"
#include "optional.h"

namespace datadog {
namespace tracing {

class DictReader;
class DictWriter;

class HTTPClient {
 public:
  struct URL {
    std::string scheme;     // http, https, or unix
    std::string authority;  // domain:port or /path/to/socket
    std::string path;       // resource, e.g. /v0.4/traces

    static Expected<HTTPClient::URL> parse(StringView input);
  };

  using HeadersSetter = std::function<void(DictWriter& headers)>;
  using ResponseHandler = std::function<void(
      int status, const DictReader& headers, std::string body)>;
  // `ErrorHandler` is for errors encountered by `HTTPClient`, not for
  // error-indicating HTTP responses.
  using ErrorHandler = std::function<void(Error)>;

  // Send a POST request to the specified `url`.  Set request headers by calling
  // the specified `set_headers` callback.  Include the specified `body` at the
  // end of the request.  Invoke the specified `on_response` callback if/when
  // a response is delivered (even if that response contains an error HTTP
  // response status).  Invoke the specified `on_error` if an error occurs
  // outside of HTTP, such as a connection failure.  If an error occurs while
  // preparing the request, return an `Error`. The behavior is undefined if
  // either of `on_response` or `on_error` throws an exception.
  virtual Expected<void> post(
      const URL& url, HeadersSetter set_headers, std::string body,
      ResponseHandler on_response, ErrorHandler on_error,
      std::chrono::steady_clock::time_point deadline) = 0;

  // Wait until there are no more outstanding requests, or until the specified
  // `deadline`.
  virtual void drain(std::chrono::steady_clock::time_point deadline) = 0;

  // Return a JSON representation of this object's configuration. The JSON
  // representation is an object with the following properties:
  //
  // - "type" is the unmangled, qualified name of the most-derived class, e.g.
  //   "datadog::tracing::Curl".
  // - "config" is an object containing this object's configuration. "config"
  //   may be omitted if the derived class has no configuration.
  virtual nlohmann::json config_json() const = 0;

  virtual ~HTTPClient() = default;
};

}  // namespace tracing
}  // namespace datadog
