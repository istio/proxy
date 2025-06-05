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

#include "proto_field_extraction/field_value_extractor/field_value_extractor.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/type.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/wire_format_lite.h"

namespace google::protobuf::field_extraction {

namespace {

using ::google::protobuf::Field;
using ::google::protobuf::Type;
using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CodedInputStream;

const char kTimestampTypeUrl[] =
    "type.googleapis.com/google.protobuf.Timestamp";

// Reads a primitive field value and converts it to a string.
template <typename CType, enum WireFormatLite::FieldType DeclaredType>
std::string ReadSingularPrimitiveField(CodedInputStream* input_stream) {
  CType value;
  WireFormatLite::ReadPrimitive<CType, DeclaredType>(input_stream, &value);
  return absl::StrCat(value);
}

// Reads a repeated primitive `field`, based on whether the field is packed or
// non-packed encoding and puts the value(s) into `*result`.
template <typename CType, enum WireFormatLite::FieldType DeclaredType>
absl::Status ReadRepeatedPrimitiveField(const Field& field,
                                        CodedInputStream* input_stream,
                                        std::vector<std::string>* result) {
  if (field.packed()) {
    // [Packed Encodeing]
    google::protobuf::RepeatedField<CType> repeated_values;
    if (WireFormatLite::ReadPackedPrimitive<CType, DeclaredType>(
            input_stream, &repeated_values)) {
      for (const auto& value : repeated_values) {
        result->push_back(absl::StrCat(value));
      }
    } else {
      return absl::InternalError(
          "Failed to read pack primitive from request proto");
    }
  } else {
    // [Non-packed Encoding]
    // Reads only one single value, not all values in the repeated field.
    result->push_back(
        ReadSingularPrimitiveField<CType, DeclaredType>(input_stream));
  }
  return absl::OkStatus();
}

// Reads a repeated primitive `field` based on different supported field type
// and calls generic `ReadRepeatedPrimitiveField<>` above with corresponding
// CType and Declared Type, then puts the values into `*result`.
absl::Status ReadRepeatedPrimitiveField(const Field& field,
                                        CodedInputStream* input_stream,
                                        std::vector<std::string>* result) {
  switch (field.kind()) {
    case Field::TYPE_INT32:
      return ReadRepeatedPrimitiveField<int32_t, WireFormatLite::TYPE_INT32>(
          field, input_stream, result);
    case Field::TYPE_UINT32:
      return ReadRepeatedPrimitiveField<uint32_t, WireFormatLite::TYPE_UINT32>(
          field, input_stream, result);
    case Field::TYPE_SINT32:
      return ReadRepeatedPrimitiveField<int32_t, WireFormatLite::TYPE_SINT32>(
          field, input_stream, result);
    case Field::TYPE_INT64:
      return ReadRepeatedPrimitiveField<int64_t, WireFormatLite::TYPE_INT64>(
          field, input_stream, result);
    case Field::TYPE_UINT64:
      return ReadRepeatedPrimitiveField<uint64_t, WireFormatLite::TYPE_UINT64>(
          field, input_stream, result);
    case Field::TYPE_SINT64:
      return ReadRepeatedPrimitiveField<int64_t, WireFormatLite::TYPE_SINT64>(
          field, input_stream, result);
    case Field::TYPE_FIXED32:
      return ReadRepeatedPrimitiveField<uint32_t, WireFormatLite::TYPE_FIXED32>(
          field, input_stream, result);
    case Field::TYPE_SFIXED32:
      return ReadRepeatedPrimitiveField<int32_t, WireFormatLite::TYPE_SFIXED32>(
          field, input_stream, result);
    case Field::TYPE_FLOAT:
      return ReadRepeatedPrimitiveField<float, WireFormatLite::TYPE_FLOAT>(
          field, input_stream, result);
    case Field::TYPE_FIXED64:
      return ReadRepeatedPrimitiveField<uint64_t, WireFormatLite::TYPE_FIXED64>(
          field, input_stream, result);
    case Field::TYPE_SFIXED64:
      return ReadRepeatedPrimitiveField<int64_t, WireFormatLite::TYPE_SFIXED64>(
          field, input_stream, result);
    case Field::TYPE_DOUBLE:
      return ReadRepeatedPrimitiveField<double, WireFormatLite::TYPE_DOUBLE>(
          field, input_stream, result);
    default:
      // This case should never happened since the supported field type
      // validation already be covered when
      // `FieldValueExtractorFactory::Create()` creates the
      // FieldExtractor.
      return absl::InternalError(
          absl::StrCat("Unexpected field type for repeated primitive field: ",
                       field.name()));
  }
}

// Reads a primitive `field` based on different supported field type and calls
// generic `ReadPrimitiveToString<>` above with corresponding CType and Declared
// Type, then returns a string.
absl::StatusOr<std::string> ReadSingularPrimitiveField(
    const Field& field, CodedInputStream* input_stream) {
  switch (field.kind()) {
    case Field::TYPE_INT32:
      return ReadSingularPrimitiveField<int32_t, WireFormatLite::TYPE_INT32>(
          input_stream);
    case Field::TYPE_UINT32:
      return ReadSingularPrimitiveField<uint32_t, WireFormatLite::TYPE_UINT32>(
          input_stream);
    case Field::TYPE_SINT32:
      return ReadSingularPrimitiveField<int32_t, WireFormatLite::TYPE_SINT32>(
          input_stream);
    case Field::TYPE_INT64:
      return ReadSingularPrimitiveField<int64_t, WireFormatLite::TYPE_INT64>(
          input_stream);
    case Field::TYPE_UINT64:
      return ReadSingularPrimitiveField<uint64_t, WireFormatLite::TYPE_UINT64>(
          input_stream);
    case Field::TYPE_SINT64:
      return ReadSingularPrimitiveField<int64_t, WireFormatLite::TYPE_SINT64>(
          input_stream);
    case Field::TYPE_FIXED32:
      return ReadSingularPrimitiveField<uint32_t, WireFormatLite::TYPE_FIXED32>(
          input_stream);
    case Field::TYPE_SFIXED32:
      return ReadSingularPrimitiveField<int32_t, WireFormatLite::TYPE_SFIXED32>(
          input_stream);
    case Field::TYPE_FLOAT:
      return ReadSingularPrimitiveField<float, WireFormatLite::TYPE_FLOAT>(
          input_stream);
    case Field::TYPE_FIXED64:
      return ReadSingularPrimitiveField<uint64_t, WireFormatLite::TYPE_FIXED64>(
          input_stream);
    case Field::TYPE_SFIXED64:
      return ReadSingularPrimitiveField<int64_t, WireFormatLite::TYPE_SFIXED64>(
          input_stream);
    case Field::TYPE_DOUBLE:
      return ReadSingularPrimitiveField<double, WireFormatLite::TYPE_DOUBLE>(
          input_stream);
    default:
      // This case should never happened since the supported field type
      // validation already be covered when
      // `FieldValueExtractorFactory::Create()` creates the
      // FieldExtractor.
      return absl::InternalError(
          absl::StrCat("Unexpected field type for repeated primitive field: ",
                       field.name()));
  }
}

// Reads a Timestamp field message type.
void ReadTimestampMessage(CodedInputStream* input_stream,
                          std::string* serialized_timestamp) {
  uint32_t length;
  input_stream->ReadVarint32(&length);
  input_stream->ReadString(serialized_timestamp, length);
}

// Finds the last value of the non-repeated field after the first value.
// Returns an empty string if there is only one value. Returns an error if the
// resource is malformed in case that the search goes forever.
absl::StatusOr<std::string> FindSingularLastValue(
    const Field* field, CodedInputStream* input_stream) {
  std::string resource;
  int position = input_stream->CurrentPosition();
  while (FieldExtractor::SearchField(*field, input_stream)) {
    if (input_stream->CurrentPosition() == position) {
      return absl::InvalidArgumentError(
          "The request message is malformed with endless values for a "
          "single field.");
    }
    position = input_stream->CurrentPosition();
    if (field->kind() == Field::TYPE_STRING) {
      // [Singular String]
      WireFormatLite::ReadString(input_stream, &resource);
    } else if (field->kind() != Field::TYPE_MESSAGE) {
      // [Singular Primitive]
      ASSIGN_OR_RETURN(resource,
                       ReadSingularPrimitiveField(*field, input_stream));
    } else if (field->type_url() == kTimestampTypeUrl) {
      // [Singular google.protobuf.Timestamp]
      ReadTimestampMessage(input_stream, &resource);
    }
  }
  return resource;
}

// Non-repeated fields can be repeat in a wireformat, in that case use the last
// value.
//
// Quote from the go/proto-encoding:
// "Normally, an encoded message would never have more than one instance of a
// non-repeated field. However, parsers are expected to handle the case in which
// they do."
absl::StatusOr<std::string> SingularFieldUseLastValue(
    const std::string first_value, const Field* field,
    CodedInputStream* input_stream) {
  ASSIGN_OR_RETURN(std::string last_value,
                   FindSingularLastValue(field, input_stream));
  if (last_value.empty()) return first_value;
  return last_value;
}

// Extracts the entry of given leaf `field` whose type is `map` in proto
// `input_stream`.
absl::StatusOr<std::vector<Value>> ExtractMapField(
    const Field* enclosing_field, const Field* key_field,
    const Field* value_field, CodedInputStream* input_stream) {
  std::vector<Value> result;
  // Only parse the map whose key type is STRING format.
  if (key_field->kind() != Field::TYPE_STRING ||
      value_field->kind() != Field::TYPE_STRING) {
    return absl::InvalidArgumentError(
        "Only STRING key and value are supported for map field extraction.");
  }

  google::protobuf::Struct proto_struct;
  while (FieldExtractor::SearchField(*enclosing_field, input_stream)) {
    auto limit = input_stream->ReadLengthAndPushLimit();
    uint32_t tag = 0;
    std::string key;
    std::string value;
    while ((tag = input_stream->ReadTag()) != 0) {
      if (key_field->number() == WireFormatLite::GetTagFieldNumber(tag)) {
        // Got Key
        WireFormatLite::ReadString(input_stream, &key);
      } else if (value_field->number() ==
                 WireFormatLite::GetTagFieldNumber(tag)) {
        // Got Value
        WireFormatLite::ReadString(input_stream, &value);
      } else {
        WireFormatLite::SkipField(input_stream, tag);
      }
    }

    if (!key.empty()) {
      (*proto_struct.mutable_fields())[key].set_string_value(value);
    }

    input_stream->Skip(input_stream->BytesUntilLimit());
    input_stream->PopLimit(limit);
  }

  if (proto_struct.fields_size() > 0) {
    Value item;
    *item.mutable_struct_value() = proto_struct;
    result.push_back(item);
  }

  return std::move(result);
}

// Extracts the value of given `field` within `enclosing_type` from the
// proto `input_stream`. The field cardinality can be either singular or
// repeated.
//
// This function is expected to work with `FieldExtractor` defined in
// tech/internal/env/framework/field_mask/field_extractor.h to extract the
// field value specified by a field mask path.
absl::StatusOr<std::vector<Value>> ExtractLeafField(
    const Type& enclosing_type, const Field* field,
    CodedInputStream* input_stream) {
  std::vector<std::string> result;
  std::vector<Value> values;
  if (field->cardinality() == Field::CARDINALITY_REPEATED) {
    // [Repeated Field]
    uint32_t tag = 0;
    while ((tag = input_stream->ReadTag()) != 0) {
      if (field->number() == WireFormatLite::GetTagFieldNumber(tag)) {
        if (field->kind() == Field::TYPE_STRING) {
          // [Repeated String]: Repeated string field is never packed in
          // encoding.
          std::string value;
          WireFormatLite::ReadString(input_stream, &value);
          result.push_back(value);
        } else if (field->kind() != Field::TYPE_MESSAGE) {
          // [Repeated Primitive]: Considering packed/unpacked encoding.
          auto read_repeated_primitive_field_status =
              ReadRepeatedPrimitiveField(*field, input_stream, &result);
          if (!read_repeated_primitive_field_status.ok()) {
            return read_repeated_primitive_field_status;
          }
        } else if (field->type_url() == kTimestampTypeUrl) {
          // [Repeated google.protobuf.Timestamp]
          std::string value;
          ReadTimestampMessage(input_stream, &value);
          result.push_back(value);
        }
      } else {
        WireFormatLite::SkipField(input_stream, tag);
      }
    }  // end while loop
  } else {
    // [Singular Field]
    if (FieldExtractor::SearchField(*field, input_stream)) {
      std::string value;
      if (field->kind() == Field::TYPE_STRING) {
        // [Singular String]
        WireFormatLite::ReadString(input_stream, &value);
      } else if (field->kind() != Field::TYPE_MESSAGE) {
        // [Singular Primitive]
        ASSIGN_OR_RETURN(value,
                         ReadSingularPrimitiveField(*field, input_stream));
      } else if (field->type_url() == kTimestampTypeUrl) {
        // [Singular google.protobuf.Timestamp]
        ReadTimestampMessage(input_stream, &value);
      }

      ASSIGN_OR_RETURN(value,
                       SingularFieldUseLastValue(value, field, input_stream));

      result.push_back(value);
    }
  }

  for (const auto& str : result) {
    Value item;
    item.set_string_value(str);
    values.push_back(item);
  }

  return std::move(values);
}

}  // namespace

absl::StatusOr<std::vector<std::string>> FieldValueExtractor::Extract(
    const CodedInputStreamWrapperFactory& message) const {
  absl::StatusOr<std::vector<Value>> values_status_or =
      field_extractor_->ExtractRepeatedFieldInfoFlattened<Value>(
          field_path_, message, ExtractLeafField);
  if (!values_status_or.ok()) {
    return values_status_or.status();
  }

  std::vector<std::string> result;
  for (const auto& value : values_status_or.value()) {
    result.push_back(value.string_value());
  }
  return result;
}

absl::StatusOr<Value> FieldValueExtractor::ExtractValue(
    const CodedInputStreamWrapperFactory& message) const {
  ASSIGN_OR_RETURN(
      auto values,
      field_extractor_->ExtractRepeatedFieldInfoFlattened<Value>(
          field_path_, message, ExtractLeafField, ExtractMapField));
  Value result;
  for (auto& value : values) {
    result.mutable_list_value()->mutable_values()->Add(std::move(value));
  }
  return result;
}

}  // namespace google::protobuf::field_extraction
