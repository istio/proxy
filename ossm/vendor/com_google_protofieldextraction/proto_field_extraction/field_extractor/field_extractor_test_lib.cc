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

#include "proto_field_extraction/field_extractor/field_extractor_test_lib.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "proto_field_extraction/field_extractor/field_extractor.h"
#include "google/protobuf/wire_format_lite.h"

namespace google::protobuf::field_extraction::testing {

using ::google::protobuf::internal::WireFormatLite;

// Defines the functors to extract various field types used by the field
// extractor under tests.
FieldInfoExtractorFunc<std::string> GetDummyStringFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) { return std::string("dummy"); };
}

FieldInfoExtractorFunc<std::string> GetStringFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<std::string> {
    std::string result;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      uint32_t length;
      input_stream->ReadVarint32(&length);
      input_stream->ReadString(&result, length);
    }
    return result;
  };
}

FieldInfoExtractorFunc<TestEnum> GetTestEnumFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<TestEnum> {
    TestEnum result;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      uint64_t enum_number;
      input_stream->ReadVarint64(&enum_number);
      EXPECT_TRUE(TestEnum_IsValid(enum_number));
      result = static_cast<TestEnum>(enum_number);
    }
    return result;
  };
}

FieldInfoExtractorFunc<bool> GetBoolFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<bool> {
    bool result = false;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      uint64_t number;
      input_stream->ReadVarint64(&number);
      result = number > 0;
    }
    return result;
  };
}

FieldInfoExtractorFunc<double> GetDoubleFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<double> {
    double result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadRaw(&result, 8);
    }
    return result;
  };
}

FieldInfoExtractorFunc<float> GetFloatFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<float> {
    float result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadRaw(&result, 4);
    }
    return result;
  };
}

FieldInfoExtractorFunc<int64_t> GetInt64FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int64_t> {
    uint64_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadVarint64(&result);
    }
    return result;
  };
}

FieldInfoExtractorFunc<uint64_t> GetUInt64FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<uint64_t> {
    uint64_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadVarint64(&result);
    }
    return result;
  };
}

FieldInfoExtractorFunc<int> GetInt32FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int> {
    uint32_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadVarint32(&result);
    }
    return result;
  };
}

FieldInfoExtractorFunc<uint64_t> GetFixed64FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<uint64_t> {
    uint64_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadRaw(&result, 8);
    }
    return result;
  };
}

FieldInfoExtractorFunc<uint32_t> GetFixed32FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<uint32_t> {
    uint32_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadRaw(&result, 4);
    }
    return result;
  };
}

FieldInfoExtractorFunc<uint32_t> GetUInt32FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<uint32_t> {
    uint32_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadVarint32(&result);
    }
    return result;
  };
}

FieldInfoExtractorFunc<int64_t> GetSFixed64FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int64_t> {
    uint64_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadRaw(&result, 8);
    }
    return result;
  };
}

FieldInfoExtractorFunc<int32_t> GetSFixed32FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int32_t> {
    uint32_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadRaw(&result, 4);
    }
    return result;
  };
}

FieldInfoExtractorFunc<int32_t> GetSInt32FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int32_t> {
    uint32_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadVarint32(&result);
      result = (result >> 1) ^ -(result & 1);
    }
    return static_cast<int32_t>(result);
  };
}

FieldInfoExtractorFunc<int64_t> GetSInt64FieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int64_t> {
    uint64_t result = 0;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      input_stream->ReadVarint64(&result);
      result = (result >> 1) ^ -(result & 1);
    }
    return static_cast<int64_t>(result);
  };
}

FieldInfoExtractorFunc<SingularFieldTestMessage>
GetSingularMessageFieldExtractor() {
  return
      [](const Type& type, const Field* field, CodedInputStream* input_stream)
          -> absl::StatusOr<SingularFieldTestMessage> {
        SingularFieldTestMessage result;
        if (FieldExtractor::SearchField(*field, input_stream)) {
          std::string serialized_result;
          uint32_t length;
          input_stream->ReadVarint32(&length);
          input_stream->ReadString(&serialized_result, length);
          result.ParseFromString(serialized_result);
        }
        return result;
      };
}

FieldInfoExtractorFunc<std::vector<SingularFieldTestMessage>>
GetRepeatedMessageFieldExtractor() {
  return
      [](const Type& type, const Field* field, CodedInputStream* input_stream)
          -> absl::StatusOr<std::vector<SingularFieldTestMessage>> {
        std::vector<SingularFieldTestMessage> result;

        uint32_t tag = 0;
        while ((tag = input_stream->ReadTag()) != 0) {
          if (field->number() == WireFormatLite::GetTagFieldNumber(tag)) {
            EXPECT_EQ(WireFormatLite::GetTagWireType(tag),
                      WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
            SingularFieldTestMessage singular_field;
            std::string serialized_singular_field;
            uint32_t length;
            input_stream->ReadVarint32(&length);
            input_stream->ReadString(&serialized_singular_field, length);
            singular_field.ParseFromString(serialized_singular_field);

            result.push_back(singular_field);
          } else {
            WireFormatLite::SkipField(input_stream, tag);
          }
        }

        return std::move(result);
      };
}

FieldInfoExtractorFunc<std::vector<std::string>>
GetRepeatedStringFieldExtractor() {
  return
      [](const Type& type, const Field* field, CodedInputStream* input_stream)
          -> absl::StatusOr<std::vector<std::string>> {
        std::vector<std::string> result;

        uint32_t tag = 0;
        while ((tag = input_stream->ReadTag()) != 0) {
          if (field->number() == WireFormatLite::GetTagFieldNumber(tag)) {
            EXPECT_EQ(WireFormatLite::GetTagWireType(tag),
                      WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
            std::string value;
            uint32_t length;
            input_stream->ReadVarint32(&length);
            input_stream->ReadString(&value, length);
            result.push_back(value);
          } else {
            WireFormatLite::SkipField(input_stream, tag);
          }
        }
        return std::move(result);
      };
}

FieldInfoMapExtractorFunc<std::vector<std::string>>
GetRepeatedStringMapFieldExtractor() {
  return [](const Field* enclosing_field, const Field* key_field,
            const Field* value_field, CodedInputStream* input_stream)
             -> absl::StatusOr<std::vector<std::string>> {
    std::vector<std::string> result;
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
        result.push_back(key);
      }
      if (!value.empty()) {
        result.push_back(value);
      }

      input_stream->Skip(input_stream->BytesUntilLimit());
      input_stream->PopLimit(limit);
    }

    return std::move(result);
  };
}

FieldInfoExtractorFunc<std::vector<int32_t>> GetRepeatedInt32FieldExtractor() {
  return
      [](const Type& type, const Field* field, CodedInputStream* input_stream)
          -> absl::StatusOr<std::vector<int32_t>> {
        std::vector<int32_t> result;

        uint32_t tag = 0;
        while ((tag = input_stream->ReadTag()) != 0) {
          if (field->number() == WireFormatLite::GetTagFieldNumber(tag)) {
            int32_t value;
            WireFormatLite::ReadPrimitive<int32_t, WireFormatLite::TYPE_INT32>(
                input_stream, &value);
            result.push_back(value);
          } else {
            WireFormatLite::SkipField(input_stream, tag);
          }
        }
        return std::move(result);
      };
}

// Verifies that the correct input (cursor of input stream, enclosing type
// and field info) is passed to the FieldInfoExtractor. For simplicity,
// extracts the number of map entries instead of the actual map contents.
FieldInfoExtractorFunc<int64_t> GetFieldInfoCountingExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<int64_t> {
    int64_t count = 0;
    uint32_t tag;
    while ((tag = input_stream->ReadTag()) != 0) {
      if (field->number() == WireFormatLite::GetTagFieldNumber(tag)) {
        ++count;
      }
      WireFormatLite::SkipField(input_stream, tag);
    }
    return count;
  };
}

FieldInfoExtractorFunc<Any> GetAnyFieldExtractor() {
  return [](const Type& type, const Field* field,
            CodedInputStream* input_stream) -> absl::StatusOr<Any> {
    Any result;
    if (FieldExtractor::SearchField(*field, input_stream)) {
      std::string serialized_result;
      uint32_t length;
      input_stream->ReadVarint32(&length);
      input_stream->ReadString(&serialized_result, length);
      result.ParseFromString(serialized_result);
    }
    return result;
  };
}

}  // namespace google::protobuf::field_extraction::testing
