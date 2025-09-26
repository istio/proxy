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

#include "proto_field_extraction/field_value_extractor/field_value_extractor_factory.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/field_extractor/field_extractor_util.h"
#include "proto_field_extraction/field_value_extractor/field_value_extractor.h"
#include "proto_field_extraction/utils/constants.h"

namespace google::protobuf::field_extraction {
namespace {

using ::google::protobuf::Field;
using ::google::protobuf::Type;

constexpr char kFieldPathDelimiter = '.';
constexpr char kTimestampTypeUrl[] =
    "type.googleapis.com/google.protobuf.Timestamp";

// Determines whether is the given `field` is supported field type.
// Returns true, if numerical/string or google.protobuf.Timestamp message type,
// otherwise false.
bool IsSupportedFieldType(const Field& field) {
  return field.kind() == Field::TYPE_STRING ||
         field.kind() == Field::TYPE_UINT32 ||
         field.kind() == Field::TYPE_UINT64 ||
         field.kind() == Field::TYPE_INT32 ||
         field.kind() == Field::TYPE_INT64 ||
         field.kind() == Field::TYPE_SINT32 ||
         field.kind() == Field::TYPE_SINT64 ||
         field.kind() == Field::TYPE_FIXED32 ||
         field.kind() == Field::TYPE_FIXED64 ||
         field.kind() == Field::TYPE_SFIXED32 ||
         field.kind() == Field::TYPE_SFIXED64 ||
         field.kind() == Field::TYPE_FLOAT ||
         field.kind() == Field::TYPE_DOUBLE ||
         (field.kind() == Field::TYPE_MESSAGE &&
          field.type_url() == kTimestampTypeUrl);
}

absl::Status ValidateLeafNode(const Field& field) {
  // Checks whether the field type is supported.
  if (!IsSupportedFieldType(field)) {
    return absl::InvalidArgumentError(
        absl::StrCat("leaf node '", field.name(),
                     "' must be numerical/string or timestamp type"));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<FieldValueExtractorInterface>>
FieldValueExtractorFactory::Create(
    absl::string_view message_type, absl::string_view field_path,
    bool support_any, absl::string_view custom_proto_map_entry_name) const {
  if (message_type.empty()) {
    return absl::InvalidArgumentError("Empty message type");
  }

  ASSIGN_OR_RETURN(FieldMetadata field_metadata,
                   ValidateFieldPathAndCollectMetadata(
                       message_type, field_path, support_any, type_finder_,
                       custom_proto_map_entry_name));

  return std::make_unique<FieldValueExtractor>(
      std::string(field_path),
      [this, &message_type, &custom_proto_map_entry_name]() {
        return std::make_unique<FieldExtractor>(
            type_finder_(std::string(message_type)), type_finder_,
            custom_proto_map_entry_name);
      });
}

absl::StatusOr<std::unique_ptr<FieldValueExtractorInterface>>
FieldValueExtractorFactory::Create(absl::string_view message_type,
                                   absl::string_view field_path) const {
  return Create(message_type, field_path, /*support_any=*/false,
                /*custom_proto_map_entry_name=*/"");
}

absl::StatusOr<FieldMetadata>
FieldValueExtractorFactory::ValidateFieldPathAndCollectMetadata(
    absl::string_view message_type, absl::string_view field_path,
    bool support_any, TypeFindFunc type_finder,
    absl::string_view custom_proto_map_entry_name) {
  if (field_path.empty()) {
    return absl::InvalidArgumentError("Empty field path");
  }

  const Type* root_message_type = type_finder(std::string(message_type));
  if (root_message_type == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown root message type  (", message_type,
                     "). Cannot find the current message type."));
  }

  const Type* parent_message_type = root_message_type;
  const Field* current_field = nullptr;
  std::vector<std::string> field_json_names;
  std::deque<absl::string_view> field_names =
      absl::StrSplit(field_path, kFieldPathDelimiter);

  do {
    absl::string_view field_name = field_names.front();
    field_names.pop_front();
    current_field = FindField(*parent_message_type, field_name);
    if (current_field == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid fieldPath (", field_path, "): no '", field_name,
                       "' field in '", message_type, "' message"));
    }
    field_json_names.push_back(current_field->json_name());

    // Updates `parent_message_type` for the next iteration.
    parent_message_type = nullptr;
    if (current_field->kind() == Field::TYPE_MESSAGE ||
        current_field->kind() == Field::TYPE_ENUM ||
        current_field->kind() == Field::TYPE_GROUP) {
      // Looks up the type for message, enum or group (deprecated, but
      // included for safe) as only message or enumeration types has type_url
      // populated.
      // See http://google3/google/protobuf/type.proto?q=symbol:type_url
      //
      // The order in the if condition (sort of) matters as it short-circuits
      // in the most common case, e.g. message.
      parent_message_type = type_finder(current_field->type_url());
    }

    if (parent_message_type == nullptr) {
      break;
    }

    if (custom_proto_map_entry_name.empty()
            ? IsMapMessageType(parent_message_type)
            : IsMapMessageType(parent_message_type,
                               custom_proto_map_entry_name)) {
      // For map fields:
      //     map<KeyType, ValueType> map_field = 1;
      //
      // The parsed descriptor looks like:
      //     message MapFieldEntry {
      //       option map_entry = true;
      //       optional KeyType key = 1;
      //       optional KeyType value = 2;
      //     }
      //     repeated MapFieldEntry map_field = 1;
      //
      // See MessageOptions.map_entry in
      // http://google3/net/proto2/proto/descriptor.proto.
      //
      // As a result, we treat a map field as a regular message defined as
      // the above MapFieldEntry and a field mask path of "map_field" as
      // "map_field.value" to retrieve the value of the map.
      field_names.push_front(
          google::protobuf::field_extraction::kProtoMapValueFieldName);
    }
    if (support_any && IsAnyMessageType(parent_message_type)) {
      // If we see a proto buf any field, we can not continue the validation.
      // We will assume it's valid and perform runtime validation instead.
      return FieldMetadata{std::string(field_path)};
    }
  } while (!field_names.empty());

  if (!field_names.empty()) {
    // Failed to validate the entire field path.
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid non-leaf node ", current_field->name(),
        " of non message type (", current_field->type_url(), ")."));
  }

  auto status = ValidateLeafNode(*current_field);
  if (!status.ok()) {
    return status;
  }

  return FieldMetadata{absl::StrJoin(field_json_names, ".")};
}
}  // namespace google::protobuf::field_extraction
