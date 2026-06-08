/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_LEGACY_UTIL_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_LEGACY_UTIL_H_

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace protobuf {
namespace util {
namespace legacy_json_util {

// This file is the sanctioned way to access the old-style "ESF" JSON parser,
// which is being removed from Protobuf. This file is visibility restricted,
// use the new parser at  google3/third_party/protobuf/util/json_util.h instead.

struct JsonParseOptions {
  bool ignore_unknown_fields;
  bool case_insensitive_enum_parsing;
  JsonParseOptions()
      : ignore_unknown_fields(false), case_insensitive_enum_parsing(false) {}
};

struct JsonPrintOptions {
  bool add_whitespace;
  bool always_print_primitive_fields;
  bool always_print_enums_as_ints;
  bool preserve_proto_field_names;
  JsonPrintOptions()
      : add_whitespace(false),
        always_print_primitive_fields(false),
        always_print_enums_as_ints(false),
        preserve_proto_field_names(false) {}
};

typedef JsonPrintOptions JsonOptions;

absl::Status MessageToJsonString(const Message& message, std::string* output,
                                 const JsonOptions& options);

inline absl::Status MessageToJsonString(const Message& message,
                                        std::string* output) {
  return MessageToJsonString(message, output, JsonOptions());
}

absl::Status JsonStringToMessage(absl::string_view input, Message* message,
                                 const JsonParseOptions& options);

inline absl::Status JsonStringToMessage(absl::string_view input,
                                        Message* message) {
  return JsonStringToMessage(input, message, JsonParseOptions());
}

absl::Status BinaryToJsonStream(TypeResolver* resolver,
                                const std::string& type_url,
                                io::ZeroCopyInputStream* binary_input,
                                io::ZeroCopyOutputStream* json_output,
                                const JsonPrintOptions& options);

inline absl::Status BinaryToJsonStream(TypeResolver* resolver,
                                       const std::string& type_url,
                                       io::ZeroCopyInputStream* binary_input,
                                       io::ZeroCopyOutputStream* json_output) {
  return BinaryToJsonStream(resolver, type_url, binary_input, json_output,
                            JsonPrintOptions());
}

absl::Status BinaryToJsonString(TypeResolver* resolver,
                                const std::string& type_url,
                                const std::string& binary_input,
                                std::string* json_output,
                                const JsonPrintOptions& options);

inline absl::Status BinaryToJsonString(TypeResolver* resolver,
                                       const std::string& type_url,
                                       const std::string& binary_input,
                                       std::string* json_output) {
  return BinaryToJsonString(resolver, type_url, binary_input, json_output,
                            JsonPrintOptions());
}

absl::Status JsonToBinaryStream(TypeResolver* resolver,
                                const std::string& type_url,
                                io::ZeroCopyInputStream* json_input,
                                io::ZeroCopyOutputStream* binary_output,
                                const JsonParseOptions& options);

inline absl::Status JsonToBinaryStream(
    TypeResolver* resolver, const std::string& type_url,
    io::ZeroCopyInputStream* json_input,
    io::ZeroCopyOutputStream* binary_output) {
  return JsonToBinaryStream(resolver, type_url, json_input, binary_output,
                            JsonParseOptions());
}

absl::Status JsonToBinaryString(TypeResolver* resolver,
                                const std::string& type_url,
                                absl::string_view json_input,
                                std::string* binary_output,
                                const JsonParseOptions& options);

inline absl::Status JsonToBinaryString(TypeResolver* resolver,
                                       const std::string& type_url,
                                       absl::string_view json_input,
                                       std::string* binary_output) {
  return JsonToBinaryString(resolver, type_url, json_input, binary_output,
                            JsonParseOptions());
}
}  // namespace legacy_json_util
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_LEGACY_UTIL_H_
