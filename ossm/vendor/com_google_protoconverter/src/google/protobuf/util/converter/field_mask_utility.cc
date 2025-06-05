// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/field_mask_utility.h"

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/stubs/status_macros.h"
#include "google/protobuf/util/converter/utility.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

namespace {

// Appends a FieldMask path segment to a prefix.
std::string AppendPathSegmentToPrefix(absl::string_view prefix,
                                      absl::string_view segment) {
  if (prefix.empty()) {
    return std::string(segment);
  }
  if (segment.empty()) {
    return std::string(prefix);
  }
  // If the segment is a map key, appends it to the prefix without the ".".
  if (absl::StartsWith(segment, "[\"")) {
    return absl::StrCat(prefix, segment);
  }
  return absl::StrCat(prefix, ".", segment);
}

}  // namespace

std::string ConvertFieldMaskPath(const absl::string_view path,
                                 ConverterCallback converter) {
  std::string result;
  result.reserve(path.size() << 1);

  bool is_quoted = false;
  bool is_escaping = false;
  int current_segment_start = 0;

  // Loops until 1 passed the end of the input to make handling the last
  // segment easier.
  for (size_t i = 0; i <= path.size(); ++i) {
    // Outputs quoted string as-is.
    if (is_quoted) {
      if (i == path.size()) {
        break;
      }
      result.push_back(path[i]);
      if (is_escaping) {
        is_escaping = false;
      } else if (path[i] == '\\') {
        is_escaping = true;
      } else if (path[i] == '\"') {
        current_segment_start = i + 1;
        is_quoted = false;
      }
      continue;
    }
    if (i == path.size() || path[i] == '.' || path[i] == '(' ||
        path[i] == ')' || path[i] == '\"') {
      result += converter(
          path.substr(current_segment_start, i - current_segment_start));
      if (i < path.size()) {
        result.push_back(path[i]);
      }
      current_segment_start = i + 1;
    }
    if (i < path.size() && path[i] == '\"') {
      is_quoted = true;
    }
  }
  return result;
}

absl::Status DecodeCompactFieldMaskPaths(absl::string_view paths,
                                         PathSinkCallback path_sink) {
  std::stack<std::string> prefix;
  int length = paths.length();
  int previous_position = 0;
  bool in_map_key = false;
  bool is_escaping = false;
  // Loops until 1 passed the end of the input to make the handle of the last
  // segment easier.
  for (int i = 0; i <= length; ++i) {
    if (i != length) {
      // Skips everything in a map key until we hit the end of it, which is
      // marked by an un-escaped '"' immediately followed by a ']'.
      if (in_map_key) {
        if (is_escaping) {
          is_escaping = false;
          continue;
        }
        if (paths[i] == '\\') {
          is_escaping = true;
          continue;
        }
        if (paths[i] != '\"') {
          continue;
        }
        // Un-escaped '"' must be followed with a ']'.
        if (i >= length - 1 || paths[i + 1] != ']') {
          return absl::InvalidArgumentError(absl::StrCat(
              "Invalid FieldMask '", paths,
              "'. Map keys should be represented as [\"some_key\"]."));
        }
        // The end of the map key ("\"]") has been found.
        in_map_key = false;
        // Skips ']'.
        i++;
        // Checks whether the key ends at the end of a path segment.
        if (i < length - 1 && paths[i + 1] != '.' && paths[i + 1] != ',' &&
            paths[i + 1] != ')' && paths[i + 1] != '(') {
          return absl::InvalidArgumentError(absl::StrCat(
              "Invalid FieldMask '", paths,
              "'. Map keys should be at the end of a path segment."));
        }
        is_escaping = false;
        continue;
      }

      // We are not in a map key, look for the start of one.
      if (paths[i] == '[') {
        if (i >= length - 1 || paths[i + 1] != '\"') {
          return absl::InvalidArgumentError(absl::StrCat(
              "Invalid FieldMask '", paths,
              "'. Map keys should be represented as [\"some_key\"]."));
        }
        // "[\"" starts a map key.
        in_map_key = true;
        i++;  // Skips the '\"'.
        continue;
      }
      // If the current character is not a special character (',', '(' or ')'),
      // continue to the next.
      if (paths[i] != ',' && paths[i] != ')' && paths[i] != '(') {
        continue;
      }
    }
    // Gets the current segment - sub-string between previous position (after
    // '(', ')', ',', or the beginning of the input) and the current position.
    absl::string_view segment =
        paths.substr(previous_position, i - previous_position);
    std::string current_prefix = prefix.empty() ? "" : prefix.top();

    if (i < length && paths[i] == '(') {
      // Builds a prefix and save it into the stack.
      prefix.push(AppendPathSegmentToPrefix(current_prefix, segment));
    } else if (!segment.empty()) {
      // When the current character is ')', ',' or the current position has
      // passed the end of the input, builds and outputs a new paths by
      // concatenating the last prefix with the current segment.
      RETURN_IF_ERROR(
          path_sink(AppendPathSegmentToPrefix(current_prefix, segment)));
    }

    // Removes the last prefix after seeing a ')'.
    if (i < length && paths[i] == ')') {
      if (prefix.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid FieldMask '", paths,
                         "'. Cannot find matching '(' for all ')'."));
      }
      prefix.pop();
    }
    previous_position = i + 1;
  }
  if (in_map_key) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid FieldMask '", paths,
                     "'. Cannot find matching ']' for all '['."));
  }
  if (!prefix.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid FieldMask '", paths,
                     "'. Cannot find matching ')' for all '('."));
  }
  return absl::Status();
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
