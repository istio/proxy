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

#include <functional>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "testing/fuzzing/fuzztest.h"
#include "absl/functional/bind_front.h"
#include "proto_field_extraction/field_extractor/field_extractor.h"
#include "proto_field_extraction/field_extractor/field_extractor_test_lib.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/utils.h"

namespace google::protobuf::field_extraction::testing {

class FieldExtractorFuzzer {
 public:
  explicit FieldExtractorFuzzer() { InitEnv(); }

  void InitEnv() {
    auto status = TypeHelper::Create(GetTestDataFilePath(
        "test_utils/testdata/field_extractor_test_proto_descriptor.pb"));
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());

    type_finder_ = absl::bind_front(&FieldExtractorFuzzer::FindType, this);
    test_message_type_ = FindType(kFieldExtractorTestMessageTypeUrl);
    field_extractor_ =
        std::make_unique<FieldExtractor>(test_message_type_, type_finder_);
  }

  // Tries to find the Type for `type_url`.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  FieldExtractor* field_extractor() { return field_extractor_.get(); }

 private:
  // The Service definition of the testing service. We do this because it's
  // easier to get Types of protos if they are part of an 'api_service' build
  // target.
  std::unique_ptr<TypeHelper> type_helper_;
  std::function<const Type*(const std::string&)> type_finder_;

  const Type* test_message_type_;
  std::unique_ptr<FieldExtractor> field_extractor_;
};

void ExtractFields(FieldExtractorTestMessage test_message_proto) {
  FieldExtractorFuzzer fuzz;
  auto cordMessage = CordMessageData(test_message_proto.SerializeAsCord());

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<std::string>(
      "singular_field.string_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<std::string>(
      "singular_field.byte_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<bool>(
      "singular_field.bool_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetBoolFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<double>(
      "singular_field.double_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetDoubleFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int64_t>(
      "singular_field.int64_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetInt64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<uint64_t>(
      "singular_field.uint64_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetUInt64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int>(
      "singular_field.int32_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetInt32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<uint64_t>(
      "singular_field.fixed64_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetFixed64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<uint32_t>(
      "singular_field.fixed32_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetFixed32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<uint32_t>(
      "singular_field.uint32_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetUInt32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int64_t>(
      "singular_field.sfixed64_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetSFixed64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int32_t>(
      "singular_field.sfixed32_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetSFixed32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int32_t>(
      "singular_field.sint32_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetSInt32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int64_t>(
      "singular_field.sint64_field",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetSInt64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<SingularFieldTestMessage>(
      "singular_field", cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetSingularMessageFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()
                ->ExtractFieldInfo<std::vector<SingularFieldTestMessage>>(
                    "repeated_singular_fields",
                    cordMessage.CreateCodedInputStreamWrapper()->Get(),
                    GetRepeatedMessageFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractFieldInfo<int64_t>(
      "repeated_field_leaf.map_string",
      cordMessage.CreateCodedInputStreamWrapper()->Get(),
      GetFieldInfoCountingExtractor()));

  // Extract repeated fields.
  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<std::string>(
      "singular_field.string_field", cordMessage, GetStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<std::string>(
      "singular_field.byte_field", cordMessage, GetStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<bool>(
      "singular_field.bool_field", cordMessage, GetBoolFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<double>(
      "singular_field.double_field", cordMessage, GetDoubleFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<float>(
      "singular_field.float_field", cordMessage, GetFloatFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<uint64_t>(
      "singular_field.uint64_field", cordMessage, GetUInt64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<int64_t>(
      "singular_field.int64_field", cordMessage, GetInt64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<int>(
      "singular_field.int32_field", cordMessage, GetInt32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<uint64_t>(
      "singular_field.fixed64_field", cordMessage, GetFixed64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<uint32_t>(
      "singular_field.fixed32_field", cordMessage, GetFixed32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<uint32_t>(
      "singular_field.uint32_field", cordMessage, GetUInt32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<int64_t>(
      "singular_field.sfixed64_field", cordMessage,
      GetSFixed64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<int32_t>(
      "singular_field.sfixed32_field", cordMessage,
      GetSFixed32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<int32_t>(
      "singular_field.sint32_field", cordMessage, GetSInt32FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<int64_t>(
      "singular_field.sint64_field", cordMessage, GetSInt64FieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()
                ->ExtractRepeatedFieldInfo<SingularFieldTestMessage>(
                    "singular_field", cordMessage,
                    GetSingularMessageFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<Any>(
      "singular_any_field", cordMessage, GetAnyFieldExtractor()));

  EXPECT_OK(
      fuzz.field_extractor()
          ->ExtractRepeatedFieldInfo<std::vector<SingularFieldTestMessage>>(
              "repeated_singular_fields", cordMessage,
              GetRepeatedMessageFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<std::string>(
      "repeated_singular_fields.string_field", cordMessage,
      GetStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfo<std::string>(
      "repeated_field.repeated_field.name", cordMessage,
      GetStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()
                ->ExtractRepeatedFieldInfo<std::vector<std::string>>(
                    "repeated_field.repeated_field.repeated_string",
                    cordMessage, GetRepeatedStringFieldExtractor()));

  EXPECT_OK(fuzz.field_extractor()->ExtractRepeatedFieldInfoFlattened<int32_t>(
      "map_singular_field.int32_field", cordMessage,
      GetRepeatedInt32FieldExtractor()));
  EXPECT_OK(
      fuzz.field_extractor()->ExtractRepeatedFieldInfoFlattened<std::string>(
          "map_singular_field.string_field", cordMessage,
          GetRepeatedStringFieldExtractor()));

  EXPECT_OK(
      fuzz.field_extractor()->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_map_field.map_field.map_field.name", cordMessage,
          GetRepeatedStringFieldExtractor()));

  EXPECT_OK(
      fuzz.field_extractor()->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_map_field.map_field.map_field.repeated_string", cordMessage,
          GetRepeatedStringFieldExtractor()));

  EXPECT_OK(
      fuzz.field_extractor()->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_field_leaf.map_string", cordMessage,
          GetRepeatedStringFieldExtractor()));

  EXPECT_OK(
      fuzz.field_extractor()->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_field.map_string", cordMessage,
          GetRepeatedStringFieldExtractor()));
}

FUZZ_TEST(FieldExtractorFuzzTest, ExtractFields)
    .WithDomains(fuzztest::Arbitrary<FieldExtractorTestMessage>());

}  // namespace google::protobuf::field_extraction::testing
