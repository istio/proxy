#include "parse_util.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "error.h"
#include "string_util.h"

namespace datadog {
namespace tracing {
namespace {

template <typename Integer>
Expected<Integer> parse_integer(StringView input, int base, StringView kind) {
  Integer value;
  const char *const end = input.data() + input.size();
  const auto status = std::from_chars(input.data(), end, value, base);
  if (status.ec == std::errc::invalid_argument) {
    std::string message;
    message += "Is not a valid integer: \"";
    append(message, input);
    message += '\"';
    return Error{Error::INVALID_INTEGER, std::move(message)};
  } else if (status.ptr != end) {
    std::string message;
    message += "Integer has trailing characters in: \"";
    append(message, input);
    message += '\"';
    return Error{Error::INVALID_INTEGER, std::move(message)};
  } else if (status.ec == std::errc::result_out_of_range) {
    std::string message;
    message += "Integer is not within the range of ";
    append(message, kind);
    message += ": ";
    append(message, input);
    return Error{Error::OUT_OF_RANGE_INTEGER, std::move(message)};
  }
  return value;
}

}  // namespace

bool falsy(StringView input) {
  auto lower = std::string{input};
  to_lower(lower);
  return lower == "0" || lower == "false" || lower == "no";
}

Expected<std::uint64_t> parse_uint64(StringView input, int base) {
  return parse_integer<std::uint64_t>(input, base, "64-bit unsigned");
}

Expected<int> parse_int(StringView input, int base) {
  return parse_integer<int>(input, base, "int");
}

Expected<double> parse_double(StringView input) {
  // This function uses a different technique from `parse_integer`, because
  // some compilers with _partial_ support for C++17 do not implement the
  // floating point portions of `std::from_chars`:
  // <https://en.cppreference.com/w/cpp/compiler_support/17#C.2B.2B17_library_features>.
  // As an alternative, we could use either of `std::stod` or `std::istream`.
  // I choose `std::istream`.
  double value;
  std::stringstream stream;
  stream << input;
  stream >> value;

  if (!stream) {
    std::string message;
    message +=
        "Is not a valid number, or is out of the range of double precision "
        "floating point: \"";
    append(message, input);
    message += '\"';
    return Error{Error::INVALID_DOUBLE, std::move(message)};
  } else if (!stream.eof()) {
    std::string message;
    message += "Number has trailing characters in: \"";
    append(message, input);
    message += '\"';
    return Error{Error::INVALID_DOUBLE, std::move(message)};
  }

  return value;
}

// List items are separated by an optional comma (",") and any amount of
// whitespace.
// Leading and trailing whitespace is ignored.
std::vector<StringView> parse_list(StringView input) {
  using uchar = unsigned char;

  input = trim(input);
  std::vector<StringView> items;
  if (input.empty()) {
    return items;
  }

  const char *const end = input.data() + input.size();
  const char *current = input.data();

  const char *begin_delim;
  do {
    const char *begin_item =
        std::find_if(current, end, [](uchar ch) { return !std::isspace(ch); });
    begin_delim = std::find_if(begin_item, end, [](uchar ch) {
      return std::isspace(ch) || ch == ',';
    });

    items.emplace_back(begin_item, std::size_t(begin_delim - begin_item));

    const char *end_delim = std::find_if(
        begin_delim, end, [](uchar ch) { return !std::isspace(ch); });

    if (end_delim != end && *end_delim == ',') {
      ++end_delim;
    }

    current = end_delim;
  } while (begin_delim != end);

  return items;
}

Expected<std::unordered_map<std::string, std::string>> parse_tags(
    std::vector<StringView> list) {
  std::unordered_map<std::string, std::string> tags;

  std::string key;
  std::string value;

  for (const StringView &token : list) {
    const auto separator = token.find(':');

    if (separator == std::string::npos) {
      key = std::string{trim(token)};
    } else {
      key = std::string{trim(token.substr(0, separator))};
      if (key.empty()) {
        continue;
      }
      value = std::string{trim(token.substr(separator + 1))};
    }

    // If there are duplicate values, then the last one wins.
    tags.insert_or_assign(std::move(key), std::move(value));
  }

  return tags;
}

// This function scans the input string to identify a separator (',' or ' ').
// Then, split tags using the identified separator and call `parse_tags` with
// the resulting unordered_map.
//
// Why?
// RFC: DD_TAGS - support space separation
//   The trace agent parses DD_TAGS as a space separated list of tags. The
//   tracers parse this as a comma-separated list. We need to have the tracers
//   parse DD_TAGS as a space-separated list when possible so that the agent and
//   tracers can use the same DD_TAGS strings while maintaining backwards
//   compatibility with comma-separated lists.
Expected<std::unordered_map<std::string, std::string>> parse_tags(
    StringView input) {
  std::vector<StringView> tags;

  size_t beg = 0;
  const size_t end = input.size();

  char separator = 0;
  size_t i = beg;

  for (; i < end; ++i) {
    if (input[i] == ',') {
      separator = ',';
      break;
    } else if (input[i] == ' ') {
      separator = ' ';
      break;
    }
  }

  if (separator == 0) {
    goto capture_all;
  }

  for (; i < end; ++i) {
    if (input[i] == separator) {
      auto tag = input.substr(beg, i - beg);
      if (tag.size() > 0) {
        tags.emplace_back(tag);
      }
      beg = i + 1;
    }
  }

capture_all:
  auto tag = input.substr(beg, i - beg);
  if (tag.size() > 0) {
    tags.emplace_back(tag);
  }

  return parse_tags(tags);
}

}  // namespace tracing
}  // namespace datadog
