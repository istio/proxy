#pragma once

// This component provides parsing-related miscellanea.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "expected.h"
#include "string_view.h"

namespace datadog {
namespace tracing {

bool falsy(StringView input);

// Return a non-negative integer parsed from the specified `input` with respect
// to the specified `base`, or return an `Error` if no such integer can be
// parsed. It is an error unless all of `input` is consumed by the parse.
// Leading and trailing whitespace are not ignored.
Expected<std::uint64_t> parse_uint64(StringView input, int base);

// Return an integer parsed from the specified `input` with respect to the
// specified `base`, or return an `Error` if no such integer can be parsed.
Expected<int> parse_int(StringView input, int base);

// Return a floating point number parsed from the specified `input`, or return
// an `Error` if not such number can be parsed. It is an error unless all of
// `input` is consumed by the parse. Leading and trailing whitespace are not
// ignored.
Expected<double> parse_double(StringView input);

// List items are separated by an optional comma (",") and any amount of
// whitespace.
// Leading and trailing whitespace are ignored.
std::vector<StringView> parse_list(StringView input);

Expected<std::unordered_map<std::string, std::string>> parse_tags(
    std::vector<StringView> list);

Expected<std::unordered_map<std::string, std::string>> parse_tags(
    StringView input);

}  // namespace tracing
}  // namespace datadog
