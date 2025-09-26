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

#ifndef PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_UTIL_H_
#define PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_UTIL_H_

#include <functional>
#include <string>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/type.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "proto_field_extraction/utils/constants.h"

namespace google::protobuf::field_extraction {
// Represents a node in a field mask path, which contains the field at the node
// and the type of the field.
struct FieldMaskPathNode {
  const google::protobuf::Field* field;
  const google::protobuf::Type* type;
};

// Returns true when the given message `type` is a proto map message type.
bool IsMapMessageType(
    const google::protobuf::Type* type,
    absl::string_view proto_map_entry_name = kProtoMapEntryName);

// Returns true when the given message `type` is a google.protobuf.Any.
bool IsAnyMessageType(const google::protobuf::Type* type);

// Finds the field descriptor for given field name in `type`. Returns nullptr
// when the given field name is not found in the `type`.
const google::protobuf::Field* FindField(const google::protobuf::Type& type,
                                         absl::string_view field_name);

// Convert a list of `google::protbuf::Value` into a list of strings.
std::vector<absl::string_view> ConvertValuesToStrings(
    absl::Span<const google::protobuf::Value> values);

}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_UTIL_H_
