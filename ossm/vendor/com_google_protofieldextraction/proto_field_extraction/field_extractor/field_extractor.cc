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

#include "proto_field_extraction/field_extractor/field_extractor.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/substitute.h"
#include "proto_field_extraction/field_extractor/field_extractor_util.h"
#include "proto_field_extraction/utils/constants.h"

namespace google::protobuf::field_extraction {
namespace {

using ::google::protobuf::Any;
using ::google::protobuf::Field;
using ::google::protobuf::Type;
using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CodedInputStream;

}  // namespace

FieldExtractor::FieldExtractor(
    const Type* type,
    std::function<const Type*(const std::string&)> type_finder,
    absl::string_view custom_proto_map_entry_name)
    : root_type_(*ABSL_DIE_IF_NULL(type)),
      type_finder_(std::move(type_finder)),
      custom_proto_map_entry_name_(custom_proto_map_entry_name) {}

// static
bool FieldExtractor::SearchField(const Field& field_desc,
                                 CodedInputStream* input_stream) {
  uint32_t tag = 0;
  while ((tag = input_stream->ReadTag()) != 0 &&
         field_desc.number() != WireFormatLite::GetTagFieldNumber(tag)) {
    WireFormatLite::SkipField(input_stream, tag);
  }
  return tag != 0;
}

// static
absl::Status FieldExtractor::ValidateNonLeafNode(
    const FieldExtractor::FieldPathNode& node, bool allow_repeated) {
  // A non-leaf node of the field mask path must be a message field.
  // See http://google3/google/protobuf/field_mask.proto for specification.
  if (node.field->kind() != Field::TYPE_MESSAGE) {
    return absl::InvalidArgumentError(
        absl::Substitute("Field '$0' is a non-leaf node of the field mask "
                         "path but it's not of message type.",
                         node.field->name()));
  }
  // The creation of FieldPathNode already guarantees that `type` not nullptr
  // when the `field` is of message type.
  DCHECK(node.type != nullptr);
  if (!allow_repeated &&
      node.field->cardinality() == Field::CARDINALITY_REPEATED) {
    return absl::InvalidArgumentError(
        absl::Substitute("Field '$0' is a non-leaf node of the field mask "
                         "path but it's a repeated field or a map field.",
                         node.field->name()));
  }
  return absl::OkStatus();
}

absl::StatusOr<FieldExtractor::FieldPathNode>
FieldExtractor::CreateFieldPathNode(const Field& field) const {
  if (field.kind() == Field::TYPE_MESSAGE) {
    if (const Type* field_type = type_finder_(field.type_url());
        field_type != nullptr) {
      return FieldPathNode{
          &field, field_type,
          custom_proto_map_entry_name_.empty()
              ? IsMapMessageType(field_type)
              : IsMapMessageType(field_type, custom_proto_map_entry_name_),
          IsAnyMessageType(field_type)};
    }
    return absl::InvalidArgumentError(
        absl::Substitute("Cannot find the type of field '$0'.", field.name()));
  }
  return FieldPathNode{&field, /*type=*/nullptr, /*is_map=*/false,
                       /*is_any=*/false};
}

absl::StatusOr<FieldExtractor::FieldPathNode>
FieldExtractor::CreateFieldPathNode(const Type& enclosing_type,
                                    absl::string_view field_name) const {
  if (const Field* field = FindField(enclosing_type, field_name);
      field != nullptr) {
    return CreateFieldPathNode(*field);
  }
  return absl::InvalidArgumentError(
      absl::Substitute("Cannot find field '$0' in '$1' message.", field_name,
                       enclosing_type.name()));
}

absl::StatusOr<FieldExtractor::FieldPathNode> FieldExtractor::ResolveMapKeyNode(
    const FieldExtractor::FieldPathNode& map_node) const {
  DCHECK_NE(map_node.type, nullptr);
  const Field* map_key_field = FindField(*map_node.type, kProtoMapKeyFieldName);
  DCHECK_NE(map_key_field, nullptr);
  return CreateFieldPathNode(*map_key_field);
}

absl::StatusOr<FieldExtractor::FieldPathNode>
FieldExtractor::ResolveMapValueNode(
    const FieldExtractor::FieldPathNode& map_node) const {
  DCHECK_NE(map_node.type, nullptr);
  const Field* map_value_field =
      FindField(*map_node.type, kProtoMapValueFieldName);
  DCHECK_NE(map_value_field, nullptr);
  return CreateFieldPathNode(*map_value_field);
}

absl::StatusOr<const Type*> FieldExtractor::ProcessNonLeafMapNode(
    CodedInputStream* input_stream,
    const CodedInputStreamWrapperFactory& root_message,
    const FieldExtractor::FieldPathNode& map_node) const {
  ASSIGN_OR_RETURN(FieldPathNode map_value_node, ResolveMapValueNode(map_node));
  auto status = ValidateNonLeafNode(map_value_node, /*allow_repeated=*/true);
  if (!status.ok()) {
    return status;
  }
  // Move the cursor to the map value data.
  if (!SearchField(*map_value_node.field, input_stream)) {
    // return nullptr to skip empty map entry.
    return nullptr;
  }
  const Type* map_value_type = map_value_node.type;
  if (IsAnyMessageType(map_value_type)) {
    // For Any as map value, resolve the true type (base on the Any.type_url)
    // and move the cursor to point to Any.value field.
    auto any_object_limit = input_stream->ReadLengthAndPushLimit();
    ASSIGN_OR_RETURN(
        map_value_type,
        ProcessNonLeafAnyNode(input_stream, root_message, map_value_node));
    input_stream->PopLimit(any_object_limit);
  }
  return map_value_type;
}

absl::StatusOr<const Type*> FieldExtractor::ProcessNonLeafAnyNode(
    CodedInputStream* input_stream,
    const CodedInputStreamWrapperFactory& root_message,
    const FieldExtractor::FieldPathNode& any_node) const {
  // Create a new coded input stream based on to iterate the
  // any node data. We can't directly iterate on input_stream because
  // CodedInputStream does not provide a way to move the cursor back to a
  // previous location.
  //
  // Setup the start and end range of any_node_stream to the current any node.
  std::unique_ptr<CodedInputStreamWrapper> any_node_stream_wrapper =
      root_message.CreateCodedInputStreamWrapper();
  auto& any_node_stream = any_node_stream_wrapper->Get();

  int initial_pos = input_stream->CurrentPosition();
  any_node_stream.Skip(initial_pos);
  any_node_stream.PushLimit(input_stream->BytesUntilLimit());

  const Type* any_true_type = nullptr;
  bool any_value_found = false;
  uint32_t tag = 0;
  while ((any_true_type == nullptr || !any_value_found) &&
         (tag = any_node_stream.ReadTag()) != 0) {
    int tag_field_number = WireFormatLite::GetTagFieldNumber(tag);
    if (tag_field_number == Any::kTypeUrlFieldNumber) {
      std::string type_url;
      WireFormatLite::ReadString(&any_node_stream, &type_url);
      if (!type_url.empty()) {
        if (any_true_type = type_finder_(type_url); any_true_type == nullptr) {
          return absl::InvalidArgumentError(absl::Substitute(
              "Field '$0' contains invalid google.protobuf.Any instance with "
              "malformed or non-recognizable `type_url` value '$1'.",
              any_node.field->name(), type_url));
        }
      }
    } else {
      if (tag_field_number == Any::kValueFieldNumber) {
        // Point the original stream cursor to the `value` field.
        input_stream->Skip(any_node_stream.CurrentPosition() - initial_pos);
        any_value_found = true;
      }
      WireFormatLite::SkipField(&any_node_stream, tag);
    }
  }

  if (any_value_found) {
    if (any_true_type == nullptr) {
      return absl::InvalidArgumentError(absl::Substitute(
          "Field '$0' contains invalid google.protobuf.Any instance with empty "
          "`type_url` and non-empty `value`.",
          any_node.field->name()));
    }
    return any_true_type;
  }
  // Return nullptr to skip this empty proto Any field.
  return nullptr;
}

}  // namespace google::protobuf::field_extraction
