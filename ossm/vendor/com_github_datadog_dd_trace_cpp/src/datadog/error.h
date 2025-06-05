#pragma once

// This component provides a `struct`, `Error`, that represents a particular
// kind of failure together with a contextual message describing the failure.
//
// Errors are enumerated by `enum Error::Code`. `Error::Code` values are
// consistent across library versions, so integer values in error diagnostics
// can always be looked up here.
//
// `struct Error` is the error type used by the `Expected` class template.  See
// `expected.h`.

#include <iosfwd>
#include <string>

#include "string_view.h"

namespace datadog {
namespace tracing {

struct Error {
  // Don't change the integer values of the `Code` enumeration.
  // The idea is that the integer values can still be looked up across library
  // versions.
  enum Code {
    OTHER = 1,
    SERVICE_NAME_REQUIRED = 2,
    MESSAGEPACK_ENCODE_FAILURE = 3,
    CURL_REQUEST_FAILURE = 4,
    DATADOG_AGENT_NULL_HTTP_CLIENT = 5,
    DATADOG_AGENT_INVALID_FLUSH_INTERVAL = 6,
    NULL_COLLECTOR = 7,
    URL_MISSING_SEPARATOR = 8,
    URL_UNSUPPORTED_SCHEME = 9,
    URL_UNIX_DOMAIN_SOCKET_PATH_NOT_ABSOLUTE = 10,
    NO_SPAN_TO_EXTRACT = 11,
    NOT_IMPLEMENTED = 12,
    MISSING_SPAN_INJECTION_STYLE = 13,
    MISSING_SPAN_EXTRACTION_STYLE = 14,
    OUT_OF_RANGE_INTEGER = 15,
    INVALID_INTEGER = 16,
    MISSING_PARENT_SPAN_ID = 17,
    RATE_OUT_OF_RANGE = 18,
    TRACE_TAGS_EXCEED_MAXIMUM_LENGTH = 19,
    INCONSISTENT_EXTRACTION_STYLES = 20,
    MAX_PER_SECOND_OUT_OF_RANGE = 21,
    MALFORMED_TRACE_TAGS = 22,
    UNKNOWN_PROPAGATION_STYLE = 23,
    TAG_MISSING_SEPARATOR = 24,
    RULE_PROPERTY_WRONG_TYPE = 25,
    RULE_TAG_WRONG_TYPE = 26,
    RULE_WRONG_TYPE = 27,
    TRACE_SAMPLING_RULES_INVALID_JSON = 28,
    TRACE_SAMPLING_RULES_WRONG_TYPE = 29,
    TRACE_SAMPLING_RULES_SAMPLE_RATE_WRONG_TYPE = 30,
    TRACE_SAMPLING_RULES_UNKNOWN_PROPERTY = 31,
    SPAN_SAMPLING_RULES_INVALID_JSON = 32,
    SPAN_SAMPLING_RULES_WRONG_TYPE = 33,
    SPAN_SAMPLING_RULES_SAMPLE_RATE_WRONG_TYPE = 34,
    SPAN_SAMPLING_RULES_UNKNOWN_PROPERTY = 35,
    SPAN_SAMPLING_RULES_MAX_PER_SECOND_WRONG_TYPE = 36,
    SPAN_SAMPLING_RULES_FILE_IO = 37,
    CURL_REQUEST_SETUP_FAILED = 38,
    CURL_HTTP_CLIENT_SETUP_FAILED = 39,
    CURL_HTTP_CLIENT_NOT_RUNNING = 40,
    CURL_HTTP_CLIENT_ERROR = 41,
    INVALID_DOUBLE = 42,
    MISSING_TRACE_ID = 43,
    ENVOY_HTTP_CLIENT_FAILURE = 44,
    MULTIPLE_PROPAGATION_STYLE_ENVIRONMENT_VARIABLES = 45,
    DUPLICATE_PROPAGATION_STYLE = 46,
    ZERO_TRACE_ID = 47,
    CURL_DEADLINE_EXCEEDED_BEFORE_REQUEST_START = 48,
    DATADOG_AGENT_INVALID_REQUEST_TIMEOUT = 49,
    DATADOG_AGENT_INVALID_SHUTDOWN_TIMEOUT = 50,
    DATADOG_AGENT_INVALID_REMOTE_CONFIG_POLL_INTERVAL = 51,
    SAMPLING_DELEGATION_RESPONSE_INVALID_JSON = 52,
  };

  Code code;
  std::string message;

  Error with_prefix(StringView) const;
};

std::ostream& operator<<(std::ostream&, const Error&);

}  // namespace tracing
}  // namespace datadog
