#pragma once

#include <string>

#include "string_view.h"

namespace datadog {
namespace tracing {

// Return the result of decoding the specified padded base64-encoded `input`. If
// `input` is not padded, then return the empty string instead.
std::string base64_decode(StringView input);

}  // namespace tracing
}  // namespace datadog
