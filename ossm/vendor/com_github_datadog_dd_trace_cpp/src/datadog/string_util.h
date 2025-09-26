#pragma once

#include <string>
#include <unordered_map>

#include "propagation_style.h"
#include "string_view.h"

namespace datadog {
namespace tracing {

// Convert the specified `text` to lower case in-place.
void to_lower(std::string& text);
std::string to_lower(StringView sv);

// Return a string representation of the specified boolean `value`.
// The result is "true" for `true` and "false" for `false`.
std::string to_string(bool b);

// Converts a double value to a string
std::string to_string(double d, size_t precision);

// Joins elements of a vector into a single string with a specified separator
std::string join(const std::vector<StringView>& values, StringView separator);

// Joins propagation styles into a single comma-separated string
std::string join_propagation_styles(const std::vector<PropagationStyle>&);

// Joins key-value pairs into a single comma-separated string
std::string join_tags(
    const std::unordered_map<std::string, std::string>& values);

// Return whether the specified `prefix` is a prefix of the specified `subject`.
bool starts_with(StringView subject, StringView prefix);

// Remove leading and trailing whitespace (as determined by `std::isspace`) from
// the specified `input`.
StringView trim(StringView);

}  // namespace tracing
}  // namespace datadog
