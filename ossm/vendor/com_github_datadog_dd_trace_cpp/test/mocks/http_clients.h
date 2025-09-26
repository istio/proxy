#pragma once

#include <datadog/error.h>
#include <datadog/http_client.h>
#include <datadog/optional.h>

#include <chrono>
#include <datadog/json.hpp>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "dict_readers.h"
#include "dict_writers.h"

using namespace datadog::tracing;

// `MockHTTPClient` handles at most one request (the most recent call to
// `post`), doing so in the `drain` member function.
//
// Customize the behavior of `MockHTTPClient` by setting any combination of the
// following data members:
// - `post_error`
// - `response_body`
// - `response_status`
// - `response_headers`
// - `response_error`
//
// If `response_error` is not null, then it will be delivered instead of the
// `response_body`.
struct MockHTTPClient : public HTTPClient {
  Optional<Error> post_error;
  std::ostringstream response_body;
  int response_status = -1;
  std::unordered_map<std::string, std::string> response_headers;
  Optional<Error> response_error;
  MockDictWriter request_headers;
  std::mutex mutex_;
  ResponseHandler on_response_;
  ErrorHandler on_error_;

  Expected<void> post(
      const URL&, HeadersSetter set_headers, std::string /*body*/,
      ResponseHandler on_response, ErrorHandler on_error,
      std::chrono::steady_clock::time_point /*deadline*/) override {
    std::lock_guard<std::mutex> lock{mutex_};
    if (!post_error) {
      on_response_ = on_response;
      on_error_ = on_error;
      set_headers(request_headers);
    }
    return Expected<void>(post_error);
  }

  void drain(std::chrono::steady_clock::time_point /*deadline*/) override {
    std::lock_guard<std::mutex> lock{mutex_};
    if (response_error && on_error_) {
      on_error_(*response_error);
    } else if (on_response_) {
      MockDictReader reader{response_headers};
      on_response_(response_status, reader, response_body.str());
    }
  }

  nlohmann::json config_json() const override {
    return nlohmann::json::object({{"type", "MockHTTPClient"}});
  }
};
