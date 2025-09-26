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

#ifndef PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_H_
#define PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "google/protobuf/io/coded_stream.h"
#undef RETURN_IF_ERROR
#include "ocpdiag/core/compat/status_macros.h"

namespace google::protobuf::field_extraction {

using TypeFindFunc =
    std::function<const google::protobuf::Type*(const std::string&)>;

// Class which extracts fields  based on the proto descriptor, field_path and
// message(in the form of
// google::protobuf::io::CodedInputStream/CodedInputStreamWrapperFactory).
//
// TODO(b/123221941): Change the extration implementations to iterative fashion.
// Utility class to extract information about a specific field from a proto
// message in wire format. See go/proto-encoding for proto encoding details.
class FieldExtractor {
 public:
  template <class T>
  using FieldInfoExtractionFn = std::function<absl::StatusOr<T>(
      const google::protobuf::Type&, const google::protobuf::Field*,
      google::protobuf::io::CodedInputStream*)>;

  template <class T>
  using FieldInfoMapExtractionFn = std::function<absl::StatusOr<T>(
      const google::protobuf::Field*, const google::protobuf::Field*,
      const google::protobuf::Field*, google::protobuf::io::CodedInputStream*)>;

  // custom_proto_map_entry_name is the customized protobuf map entry name. If
  // it is unspecified, it is `map_entry`.
  FieldExtractor(const google::protobuf::Type* type, TypeFindFunc type_finder,
                 absl::string_view custom_proto_map_entry_name = "");

  virtual ~FieldExtractor() = default;

  // Moves input_stream cursor to the position right after the tag of the given
  // field. Return true if the field is found in the input stream.
  //
  // The given field descriptor must represent a singular field, packed or
  // unpacked repeated field, otherwise, the behavior is undefined. In case of
  // unpacked repeated field it will move the cursor to the position of the
  // first matching tag in the stream.
  static bool SearchField(const google::protobuf::Field& field_desc,
                          google::protobuf::io::CodedInputStream* input_stream);

  // Extracts information related to given field (represented by
  // field_mask_path). The field_info_extractor will be called when reaching
  // the last element in the field_mask_path with enclosing Type descriptor,
  // Field descriptor of the last field in the path and the input stream which
  // has been adjusted to point to the *parent* message of the last field.
  //
  // See go/proto-encoding for how to parse proto fields in binary encoding
  // format.
  template <typename T>
  absl::StatusOr<T> ExtractFieldInfo(
      const std::string& field_mask_path, google::protobuf::io::CodedInputStream& message,
      const FieldInfoExtractionFn<T>& field_info_extractor) const {
    if (field_mask_path.empty()) {
      return absl::InvalidArgumentError("Field mask path cannot be empty.");
    }
    std::vector<absl::string_view> field_names =
        absl::StrSplit(field_mask_path, '.', absl::SkipWhitespace());
    return ExtractFieldInfoHelper(&message, root_type_, field_names.cbegin(),
                                  field_names.cend(),
                                  std::move(field_info_extractor));
  }

  // A version of ExtractFieldInfo which allows repeated nonleaf objects in the
  // path leading to the field of interest. One implication of this is that
  // it is always expected to return a number of elements (as opposed to single
  // element in case of ExtractFieldInfo). Therefore return type is always a
  // vector.
  template <typename T>
  absl::StatusOr<std::vector<T>> ExtractRepeatedFieldInfo(
      const std::string& field_mask_path,
      const CodedInputStreamWrapperFactory& message,
      const FieldInfoExtractionFn<T>& field_info_extractor,
      std::optional<FieldInfoMapExtractionFn<T>> field_info_map_extractor =
          std::nullopt) const {
    if (field_mask_path.empty()) {
      return absl::InvalidArgumentError("Field mask path cannot be empty.");
    }
    std::vector<absl::string_view> field_names =
        absl::StrSplit(field_mask_path, '.', absl::SkipWhitespace());

    std::vector<T> result;
    auto stream = message.CreateCodedInputStreamWrapper();
    RETURN_IF_ERROR(ExtractRepeatedFieldInfoHelper(
        &stream->Get(), message, root_type_, field_names.cbegin(),
        field_names.cend(), field_info_extractor, field_info_map_extractor,
        &result));
    return result;
  }

  // A more specialized version of ExtractRepeatedFieldInfo which can be used
  // in situations when provided field info extractor returns a vector, and
  // the caller is not interested in how elements are grouped in the tree and
  // just needs a single vector containing all extracted elements.
  // The result will be just a vector of items (as opposed to vector of vectors
  // of items if same field info extractor was used with
  // ExtractRepeatedFieldInfo function).
  template <typename T>
  absl::StatusOr<std::vector<T>> ExtractRepeatedFieldInfoFlattened(
      const std::string& field_mask_path,
      const CodedInputStreamWrapperFactory& message,
      const FieldInfoExtractionFn<std::vector<T>>& field_info_extractor,
      std::optional<FieldInfoMapExtractionFn<std::vector<T>>>
          field_info_map_extractor = std::nullopt) const {
    ASSIGN_OR_RETURN(
        std::vector<std::vector<T>> raw_result,
        ExtractRepeatedFieldInfo(field_mask_path, message, field_info_extractor,
                                 field_info_map_extractor));
    std::vector<T> result;
    for (auto& item : raw_result) {
      std::move(item.begin(), item.end(), std::back_inserter(result));
    }
    return result;
  }

 private:
  // Represents a node in the field path. For example, for a field path like
  // "ab.bc.cd", there will be three FieldPathNode objects representing "ab",
  // "bc" and "cd", repectively.
  struct FieldPathNode {
    // The proto field descriptor.
    const google::protobuf::Field* field = nullptr;
    // The proto type descriptor if this field is a message typed field.
    const google::protobuf::Type* type = nullptr;
    // Whether this field is of proto Map type.
    bool is_map = false;
    // Whether this field is of google.protobuf.Any type.
    bool is_any = false;
  };

  // Validates a non-leaf field path node, for example, the non-leaf node should
  // be of message type, etc.
  static absl::Status ValidateNonLeafNode(const FieldPathNode& node,
                                          bool allow_repeated);

  // Recurisvely search in the proto for the field and extract the desired info
  // using given field_info_extractor. Assumes that the path may NOT contain
  // repeated nonleaf objects.
  template <typename T>
  absl::StatusOr<T> ExtractFieldInfoHelper(
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Type& enclosing_type,
      const std::vector<absl::string_view>::const_iterator current,
      const std::vector<absl::string_view>::const_iterator end,
      const FieldInfoExtractionFn<T>& field_info_extractor) const {
    // Find the current field info.
    ASSIGN_OR_RETURN(FieldPathNode current_node,
                     CreateFieldPathNode(enclosing_type, *current));

    // Base case of recursion: we have reached to the last field in the path.
    if (current + 1 == end) {
      return field_info_extractor(enclosing_type, current_node.field,
                                  input_stream);
    }

    // We are in the middle and have more nodes to visit. Current node must be
    // a field with Message type.
    RETURN_IF_ERROR(
        ValidateNonLeafNode(current_node, /*allow_repeated=*/false));
    // Search in the input_stream and move the cursor to point to the data
    // segment representing `field`.
    if (!SearchField(*current_node.field, input_stream)) {
      // This field is not set in the message, return default value of T.
      return T{};
    }
    // Update input_stream for the next iteration.
    input_stream->ReadLengthAndPushLimit();
    return ExtractFieldInfoHelper(input_stream, *current_node.type, current + 1,
                                  end, std::move(field_info_extractor));
  }

  // Recursively search in the proto for the field and extract the desired info
  // using given field_info_extractor. Assumes that the path may contain
  // repeated nonleaf fields including map field.
  template <typename T>
  absl::Status ExtractRepeatedFieldInfoHelper(
      google::protobuf::io::CodedInputStream* input_stream,
      const CodedInputStreamWrapperFactory& root_message,
      const google::protobuf::Type& enclosing_type,
      const std::vector<absl::string_view>::const_iterator current,
      const std::vector<absl::string_view>::const_iterator end,
      const FieldInfoExtractionFn<T>& field_info_extractor,
      std::optional<FieldInfoMapExtractionFn<T>> field_info_map_extractor,
      std::vector<T>* results) const {
    // Find the current field info.
    ASSIGN_OR_RETURN(FieldPathNode current_node,
                     CreateFieldPathNode(enclosing_type, *current));

    // Base case of recursion: we have reached to the last field in the path.
    if (current + 1 == end) {
      if (current_node.is_map) {
        ASSIGN_OR_RETURN(auto map_key_node, ResolveMapKeyNode(current_node));
        ASSIGN_OR_RETURN(auto map_value_node,
                         ResolveMapValueNode(current_node));
        if (field_info_map_extractor.has_value() &&
            (map_key_node.field->kind() == Field::TYPE_STRING &&
             map_value_node.field->kind() == Field::TYPE_STRING)) {
          // Extract the map differently when both key and value are `string`
          // type.
          ASSIGN_OR_RETURN(auto result,
                           field_info_map_extractor.value()(
                               current_node.field, map_key_node.field,
                               map_value_node.field, input_stream));
          results->push_back(std::move(result));
        } else {
          // If the current node is a map field, move the cursor to the map
          // value which is the effective field to pass to the
          // FieldInfoExtractionFn.
          while (SearchField(*current_node.field, input_stream)) {
            auto limit = input_stream->ReadLengthAndPushLimit();
            ASSIGN_OR_RETURN(
                auto result,
                field_info_extractor(*current_node.type, map_value_node.field,
                                     input_stream));
            results->push_back(std::move(result));
            input_stream->Skip(input_stream->BytesUntilLimit());
            input_stream->PopLimit(limit);
          }
        }
      } else {
        ASSIGN_OR_RETURN(auto result,
                         field_info_extractor(
                             enclosing_type, current_node.field, input_stream));
        results->push_back(std::move(result));
      }
      return absl::OkStatus();
    }

    // We are in the middle and have more nodes to visit. Current node must be
    // a field with Message type.
    RETURN_IF_ERROR(ValidateNonLeafNode(current_node, /*allow_repeated=*/true));
    // Iterate the current input_stream range and find out all the data segments
    // of the current field. This is how repeated fields are encoded.
    while (SearchField(*current_node.field, input_stream)) {
      auto limit = input_stream->ReadLengthAndPushLimit();

      if (current_node.is_map) {
        // Find the map value node and move the input stream cursor to point to
        // the map value field.
        ASSIGN_OR_RETURN(
            const auto* map_value_type,
            ProcessNonLeafMapNode(input_stream, root_message, current_node));
        if (map_value_type != nullptr) {
          // Set iteration limit to the map value data and continue extraction
          // recursively.
          auto map_value_limit = input_stream->ReadLengthAndPushLimit();
          RETURN_IF_ERROR(ExtractRepeatedFieldInfoHelper(
              input_stream, root_message, *map_value_type, current + 1, end,
              field_info_extractor, field_info_map_extractor, results));
          input_stream->Skip(input_stream->BytesUntilLimit());
          input_stream->PopLimit(map_value_limit);
        }
        // Skip empty map entry.
      } else if (current_node.is_any) {
        // Find the Any value type and move the input stream cursor to point to
        // the map value field.
        ASSIGN_OR_RETURN(
            const auto* any_value_type,
            ProcessNonLeafAnyNode(input_stream, root_message, current_node));

        if (any_value_type != nullptr) {
          // Set iteration limit to the Any value data and continue extraction
          // recursively.
          auto any_value_limit = input_stream->ReadLengthAndPushLimit();
          RETURN_IF_ERROR(ExtractRepeatedFieldInfoHelper(
              input_stream, root_message, *any_value_type, current + 1, end,
              field_info_extractor, field_info_map_extractor, results));
          input_stream->Skip(input_stream->BytesUntilLimit());
          input_stream->PopLimit(any_value_limit);
        }
        // Skip empty (e.g. no `value` data) or invalid (e.g. no `type_url`
        // specified) proto Any objects.
      } else {
        // Normal message typed field.
        RETURN_IF_ERROR(ExtractRepeatedFieldInfoHelper(
            input_stream, root_message, *current_node.type, current + 1, end,
            field_info_extractor, field_info_map_extractor, results));
      }

      input_stream->Skip(input_stream->BytesUntilLimit());
      input_stream->PopLimit(limit);
    }

    return absl::OkStatus();
  }

  // Creates the field path node with resolved type related info.
  absl::StatusOr<FieldPathNode> CreateFieldPathNode(
      const google::protobuf::Type& enclosing_type,
      absl::string_view field_name) const;
  absl::StatusOr<FieldPathNode> CreateFieldPathNode(
      const google::protobuf::Field& field) const;

  // Returns the field path node of the map key field from the given proto map
  // field node. Proto map field is encoded as a repeated field of MapFieldEntry
  // message on the wire.
  //
  // For map fields:
  //     map<KeyType, ValueType> map_field = 1;
  //
  // The parsed descriptor looks like:
  //     message MapFieldEntry {
  //       option map_entry = true;
  //       optional KeyType key = 1;
  //       optional ValueType value = 2;
  //     }
  //     repeated MapFieldEntry map_field = 1;
  //
  // See MessageOptions.map_entry in
  // http://google3/net/proto2/proto/descriptor.proto.
  absl::StatusOr<FieldPathNode> ResolveMapKeyNode(
      const FieldPathNode& map_node) const;

  // Returns the field path node of the map value field from the given proto map
  // field node.
  absl::StatusOr<FieldPathNode> ResolveMapValueNode(
      const FieldPathNode& map_node) const;

  // Move the input stream cursor to point to the map value field and return
  // the resolved map value type. Returns nullptr for empty map entry (i.e. map
  // entry with not map value).
  absl::StatusOr<const google::protobuf::Type*> ProcessNonLeafMapNode(
      google::protobuf::io::CodedInputStream* input_stream,
      const CodedInputStreamWrapperFactory& root_message,
      const FieldPathNode& map_node) const;

  // Move the input stream cursor to point to the Any value field and return
  // the resolved true type of the underlying message value. Returns nullptr
  // for empty (e.g. no `value` data) or invalid (e.g. no `type_url` specified)
  // proto Any objects.
  absl::StatusOr<const google::protobuf::Type*> ProcessNonLeafAnyNode(
      google::protobuf::io::CodedInputStream* input_stream,
      const CodedInputStreamWrapperFactory& root_message,
      const FieldPathNode& any_node) const;

  const google::protobuf::Type& root_type_;
  const TypeFindFunc type_finder_;

  // Non-default ProtoMapEntryName used in proto field lookup.
  std::string custom_proto_map_entry_name_;

  FieldExtractor(const FieldExtractor&) = delete;
  FieldExtractor& operator=(const FieldExtractor&) = delete;
};

#undef RETURN_IF_ERROR
}  // namespace google::protobuf::field_extraction

#endif  // PROTO_FIELD_EXTRACTION_SRC_FIELD_EXTRACTOR_FIELD_EXTRACTOR_H_
