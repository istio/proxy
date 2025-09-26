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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "proto_field_extraction/field_extractor/field_extractor_test_lib.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/testdata/field_extractor_test.pb.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "ocpdiag/core/testing/proto_matchers.h"
#include "ocpdiag/core/testing/status_matchers.h"
#include "google/protobuf/io/coded_stream.h"

namespace google::protobuf::field_extraction {
namespace testing {
namespace {

using ::google::protobuf::Any;
using ::google::protobuf::Field;
using ::google::protobuf::Type;
using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CodedInputStream;
using ::ocpdiag::testing::EqualsProto;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::ocpdiag::testing::IsOkAndHolds;
using ::ocpdiag::testing::StatusIs;

}  // namespace

class FieldExtractorTest : public ::testing::Test {
 protected:
  FieldExtractorTest() = default;

  void SetUp() override {
    ASSERT_OK(GetTextProto(
        GetTestDataFilePath("test_utils/testdata/"
                            "field_extractor_test_message.proto.txt"),
        &test_message_proto_));
    message_data_ = std::make_unique<CordMessageData>(
        test_message_proto_.SerializeAsCord());

    auto status = TypeHelper::Create(GetTestDataFilePath(
        "test_utils/testdata/field_extractor_test_proto_descriptor.pb"));
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());

    test_message_type_ = FindType(kFieldExtractorTestMessageTypeUrl);
    ASSERT_NE(test_message_type_, nullptr);

    type_finder_ = absl::bind_front(&FieldExtractorTest::FindType, this);
    test_message_type_ = FindType(kFieldExtractorTestMessageTypeUrl);
    field_extractor_ =
        std::make_unique<FieldExtractor>(test_message_type_, type_finder_);
  }

  // Tries to find the Type for `type_url`.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  // The Service definition of the testing service. We do this because it's
  // easier to get Types of protos if they are part of an 'api_service' build
  // target.

  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  std::function<const Type*(const std::string&)> type_finder_;

  testing::FieldExtractorTestMessage test_message_proto_;
  std::unique_ptr<CordMessageData> message_data_ = nullptr;

  const Type* test_message_type_;
  std::unique_ptr<FieldExtractor> field_extractor_;
};

TEST_F(FieldExtractorTest, EmptyFieldMaskPath) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<std::string>(
                  "", message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetDummyStringFieldExtractor()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Field mask path cannot be empty."));
}

TEST_F(FieldExtractorTest, UnknownField) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<std::string>(
          "unknown.field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetDummyStringFieldExtractor()),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "Cannot find field 'unknown' in "
          "'google.protobuf.field_extraction.testing.FieldExtractorTestMessage'"
          " message."));
}

TEST_F(FieldExtractorTest, InvalidNonLeafPrimitiveTypeField) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<std::string>(
                  "repeated_field_leaf.repeated_string.unknown",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetDummyStringFieldExtractor()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Field 'repeated_string' is a non-leaf node of the "
                       "field mask path but it's not of message type."));
}

TEST_F(FieldExtractorTest, InvalidNonLeafRepeatedField) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<std::string>(
          "repeated_singular_fields.string_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetDummyStringFieldExtractor()),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "Field 'repeated_singular_fields' is a non-leaf node of "
          "the field mask path but it's a repeated field or a map field."));
}

TEST_F(FieldExtractorTest, InvalidTypeFinder) {
  field_extractor_ = std::make_unique<FieldExtractor>(
      test_message_type_, [](const std::string& type_url) { return nullptr; });

  EXPECT_THAT(field_extractor_->ExtractFieldInfo<std::string>(
                  "singular_field.string_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetDummyStringFieldExtractor()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Cannot find the type of field 'singular_field'."));
}

TEST_F(FieldExtractorTest, ExtractString) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<std::string>(
          "singular_field.string_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetStringFieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().string_field()));
}

TEST_F(FieldExtractorTest, ExtractByte) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<std::string>(
                  "singular_field.byte_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetStringFieldExtractor()),
              IsOkAndHolds(test_message_proto_.singular_field().byte_field()));
}

TEST_F(FieldExtractorTest, ExtractEnum) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<TestEnum>(
                  "singular_field.enum_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetTestEnumFieldExtractor()),
              IsOkAndHolds(test_message_proto_.singular_field().enum_field()));
}

TEST_F(FieldExtractorTest, ExtractBool) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<bool>(
                  "singular_field.bool_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetBoolFieldExtractor()),
              IsOkAndHolds(test_message_proto_.singular_field().bool_field()));
}

TEST_F(FieldExtractorTest, ExtractDouble) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<double>(
          "singular_field.double_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetDoubleFieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().double_field()));
}

TEST_F(FieldExtractorTest, ExtractFloat) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<float>(
                  "singular_field.float_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetFloatFieldExtractor()),
              IsOkAndHolds(test_message_proto_.singular_field().float_field()));
}

TEST_F(FieldExtractorTest, ExtractInt64) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<int64_t>(
                  "singular_field.int64_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetInt64FieldExtractor()),
              IsOkAndHolds(test_message_proto_.singular_field().int64_field()));
}

TEST_F(FieldExtractorTest, ExtractUint64) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<uint64_t>(
          "singular_field.uint64_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetUInt64FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().uint64_field()));
}

TEST_F(FieldExtractorTest, ExtractInt32) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<int>(
                  "singular_field.int32_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetInt32FieldExtractor()),
              IsOkAndHolds(test_message_proto_.singular_field().int32_field()));
}

TEST_F(FieldExtractorTest, ExtractFixed64) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<uint64_t>(
          "singular_field.fixed64_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetFixed64FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().fixed64_field()));
}

TEST_F(FieldExtractorTest, ExtractFixed32) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<uint32_t>(
          "singular_field.fixed32_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetFixed32FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().fixed32_field()));
}

TEST_F(FieldExtractorTest, ExtractUint32) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<uint32_t>(
          "singular_field.uint32_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetUInt32FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().uint32_field()));
}

TEST_F(FieldExtractorTest, ExtractSfixed64) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<int64_t>(
          "singular_field.sfixed64_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetSFixed64FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().sfixed64_field()));
}

TEST_F(FieldExtractorTest, ExtractSfixed32) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<int32_t>(
          "singular_field.sfixed32_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetSFixed32FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().sfixed32_field()));
}

TEST_F(FieldExtractorTest, ExtractSint32) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<int32_t>(
          "singular_field.sint32_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetSInt32FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().sint32_field()));
}

TEST_F(FieldExtractorTest, ExtractSint64) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<int64_t>(
          "singular_field.sint64_field",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetSInt64FieldExtractor()),
      IsOkAndHolds(test_message_proto_.singular_field().sint64_field()));
}

TEST_F(FieldExtractorTest, ExtractLeafMessage) {
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<SingularFieldTestMessage>(
                  "singular_field",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  GetSingularMessageFieldExtractor()),
              IsOkAndHolds(EqualsProto(test_message_proto_.singular_field())));
}

TEST_F(FieldExtractorTest, ExtractLeafRepeatedMessage) {
  EXPECT_THAT(
      field_extractor_->ExtractFieldInfo<std::vector<SingularFieldTestMessage>>(
          "repeated_singular_fields",
          message_data_->CreateCodedInputStreamWrapper()->Get(),
          GetRepeatedMessageFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          EqualsProto(test_message_proto_.repeated_singular_fields(0)),
          EqualsProto(test_message_proto_.repeated_singular_fields(1)),
          EqualsProto(test_message_proto_.repeated_singular_fields(2)))));
}

TEST_F(FieldExtractorTest, ExtractLeafMap) {
  // Verifies that the correct input (cursor of input stream, enclosing type
  // and field info) is passed to the FieldInfoExtractor. For simplicity,
  // extracts the number of map entries instead of the actual map contents.
  auto field_info_counting_extractor =
      [](const Type& type, const Field* field,
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

  EXPECT_THAT(field_extractor_->ExtractFieldInfo<int64_t>(
                  "repeated_field_leaf.map_string",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_double",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_float",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_int64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_int32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_fixed64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_fixed32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_uint32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sfixed64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sfixed32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sint32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sint64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_int64_int64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_int32_int32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_fixed64_fixed64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_fixed32_fixed32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_uint32_uint32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sfixed64_sfixed64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sfixed32_sfixed32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sint32_sint32",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
  EXPECT_THAT(field_extractor_->ExtractFieldInfo<double>(
                  "repeated_field_leaf.map_sint64_sint64",
                  message_data_->CreateCodedInputStreamWrapper()->Get(),
                  field_info_counting_extractor),
              IsOkAndHolds(2));
}

// All the relevant test cases for nonrepeated FieldExtractor should pass for
// repeated field extractor, plus some specific cases involving repeated
// fields.
using RepeatedFieldExtractorTest = FieldExtractorTest;

TEST_F(RepeatedFieldExtractorTest, InvalidAnyTypeUrl) {
  ASSERT_TRUE(test_message_proto_.mutable_singular_any_field()->PackFrom(
      test_message_proto_.singular_field()));
  test_message_proto_.mutable_singular_any_field()->set_type_url(
      "invalid-any-type-url");

  message_data_->Cord().Append(test_message_proto_.SerializeAsCord());

  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "singular_any_field.string_field", *message_data_,
          GetStringFieldExtractor()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Field 'singular_any_field' contains invalid "
               "google.protobuf.Any instance with malformed or "
               "non-recognizable `type_url` value 'invalid-any-type-url'."));
}

TEST_F(RepeatedFieldExtractorTest, EmptyFieldMaskPath) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "", *message_data_, GetDummyStringFieldExtractor()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Field mask path cannot be empty."));
}

TEST_F(RepeatedFieldExtractorTest, UnknownField) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "unknown.field", *message_data_, GetDummyStringFieldExtractor()),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "Cannot find field 'unknown' in "
          "'google.protobuf.field_extraction.testing.FieldExtractorTestMessage'"
          " message."));
}

TEST_F(RepeatedFieldExtractorTest, InvalidNonLeafPrimitiveTypeField) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "repeated_field.repeated_string.unknown", *message_data_,
                  GetDummyStringFieldExtractor()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Field 'repeated_string' is a non-leaf node of the "
                       "field mask path but it's not of message type."));
}

TEST_F(RepeatedFieldExtractorTest, InvalidTypeFinder) {
  field_extractor_ = std::make_unique<FieldExtractor>(
      test_message_type_, [](const std::string& type_url) { return nullptr; });
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "repeated_field.name", *message_data_,
                  GetDummyStringFieldExtractor()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Cannot find the type of field 'repeated_field'."));
}

TEST_F(RepeatedFieldExtractorTest, ExtractString) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "singular_field.string_field", *message_data_,
                  GetStringFieldExtractor()),
              IsOkAndHolds(std::vector<std::string>(
                  {test_message_proto_.singular_field().string_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractByte) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "singular_field.byte_field", *message_data_,
                  GetStringFieldExtractor()),
              IsOkAndHolds(std::vector<std::string>(
                  {test_message_proto_.singular_field().byte_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractEnum) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<TestEnum>(
                  "singular_field.enum_field", *message_data_,
                  GetTestEnumFieldExtractor()),
              IsOkAndHolds(std::vector<TestEnum>(
                  {test_message_proto_.singular_field().enum_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractBool) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<bool>(
          "singular_field.bool_field", *message_data_, GetBoolFieldExtractor()),
      IsOkAndHolds(std::vector<bool>(
          {test_message_proto_.singular_field().bool_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractDouble) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<double>(
                  "singular_field.double_field", *message_data_,
                  GetDoubleFieldExtractor()),
              IsOkAndHolds(std::vector<double>(
                  {test_message_proto_.singular_field().double_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractFloat) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<float>(
                  "singular_field.float_field", *message_data_,
                  GetFloatFieldExtractor()),
              IsOkAndHolds(std::vector<float>(
                  {test_message_proto_.singular_field().float_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractUint64) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint64_t>(
                  "singular_field.uint64_field", *message_data_,
                  GetUInt64FieldExtractor()),
              IsOkAndHolds(std::vector<uint64_t>(
                  {test_message_proto_.singular_field().uint64_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractInt64) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int64_t>(
                  "singular_field.int64_field", *message_data_,
                  GetInt64FieldExtractor()),
              IsOkAndHolds(std::vector<int64_t>(
                  {test_message_proto_.singular_field().int64_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractInt32) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int>(
                  "singular_field.int32_field", *message_data_,
                  GetInt32FieldExtractor()),
              IsOkAndHolds(std::vector<int>(
                  {test_message_proto_.singular_field().int32_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractFixed64) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint64_t>(
                  "singular_field.fixed64_field", *message_data_,
                  GetFixed64FieldExtractor()),
              IsOkAndHolds(std::vector<uint64_t>(
                  {test_message_proto_.singular_field().fixed64_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractFixed32) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint32_t>(
                  "singular_field.fixed32_field", *message_data_,
                  GetFixed32FieldExtractor()),
              IsOkAndHolds(std::vector<uint32_t>(
                  {test_message_proto_.singular_field().fixed32_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractUint32) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint32_t>(
                  "singular_field.uint32_field", *message_data_,
                  GetUInt32FieldExtractor()),
              IsOkAndHolds(std::vector<uint32_t>(
                  {test_message_proto_.singular_field().uint32_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractSfixed64) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int64_t>(
                  "singular_field.sfixed64_field", *message_data_,
                  GetSFixed64FieldExtractor()),
              IsOkAndHolds(std::vector<int64_t>(
                  {test_message_proto_.singular_field().sfixed64_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractSfixed32) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int32_t>(
                  "singular_field.sfixed32_field", *message_data_,
                  GetSFixed32FieldExtractor()),
              IsOkAndHolds(std::vector<int32_t>(
                  {test_message_proto_.singular_field().sfixed32_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractSint32) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int32_t>(
                  "singular_field.sint32_field", *message_data_,
                  GetSInt32FieldExtractor()),
              IsOkAndHolds(std::vector<int32_t>(
                  {test_message_proto_.singular_field().sint32_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractSint64) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int64_t>(
                  "singular_field.sint64_field", *message_data_,
                  GetSInt64FieldExtractor()),
              IsOkAndHolds(std::vector<int64_t>(
                  {test_message_proto_.singular_field().sint64_field()})));
}

TEST_F(RepeatedFieldExtractorTest, ExtractMessage) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<SingularFieldTestMessage>(
          "singular_field", *message_data_, GetSingularMessageFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          EqualsProto(test_message_proto_.singular_field()))));
}

TEST_F(RepeatedFieldExtractorTest, ExtractMessage_Any) {
  test_message_proto_.mutable_singular_any_field()->PackFrom(
      test_message_proto_.singular_field());
  message_data_->Cord().Append(test_message_proto_.SerializeAsCord());

  FieldInfoExtractorFunc<Any> extactor =
      [](const Type& type, const Field* field,
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

  auto s = field_extractor_->ExtractRepeatedFieldInfo<Any>(
      "singular_any_field", *message_data_, extactor);
}

TEST_F(RepeatedFieldExtractorTest, ExtractRepeatedMessage) {
  EXPECT_THAT(
      field_extractor_
          ->ExtractRepeatedFieldInfo<std::vector<SingularFieldTestMessage>>(
              "repeated_singular_fields", *message_data_,
              GetRepeatedMessageFieldExtractor()),
      IsOkAndHolds(Contains(UnorderedElementsAre(
          EqualsProto(test_message_proto_.repeated_singular_fields(0)),
          EqualsProto(test_message_proto_.repeated_singular_fields(1)),
          EqualsProto(test_message_proto_.repeated_singular_fields(2))))));
}

TEST_F(RepeatedFieldExtractorTest, ExtractRepeatedParentSingularChild) {
  // a repeated parent (3 instances), each has 1 singular child: O* -> F
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "repeated_singular_fields.string_field", *message_data_,
          GetStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          test_message_proto_.repeated_singular_fields(0).string_field(),
          test_message_proto_.repeated_singular_fields(1).string_field(),
          test_message_proto_.repeated_singular_fields(2).string_field())));
}

TEST_F(RepeatedFieldExtractorTest,
       RepeatedGrandParentRepeatedParentSingularChild) {
  // A repeated object has a child object (which is also repeated), which in
  // turn has singular child field: O* -> O* -> F (2 * 2 * 1 = 4 instances)
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "repeated_field.repeated_field.name", *message_data_,
          GetStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          test_message_proto_.repeated_field(0).repeated_field(0).name(),
          test_message_proto_.repeated_field(0).repeated_field(1).name(),
          test_message_proto_.repeated_field(1).repeated_field(0).name(),
          test_message_proto_.repeated_field(1).repeated_field(1).name())));
}

TEST_F(RepeatedFieldExtractorTest,
       RepeatedGrandParentRepeatedParentRepeatedChild) {
  // A repeated object has a child object (which is also repeated), which in
  // turn has repeated child field: O* -> O* -> F* (2 * 2 * 2 = 8 instances)
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::vector<std::string>>(
          "repeated_field.repeated_field.repeated_string", *message_data_,
          GetRepeatedStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          UnorderedElementsAre(test_message_proto_.repeated_field(0)
                                   .repeated_field(0)
                                   .repeated_string(0),
                               test_message_proto_.repeated_field(0)
                                   .repeated_field(0)
                                   .repeated_string(1)),
          UnorderedElementsAre(test_message_proto_.repeated_field(0)
                                   .repeated_field(1)
                                   .repeated_string(0),
                               test_message_proto_.repeated_field(0)
                                   .repeated_field(1)
                                   .repeated_string(1)),
          UnorderedElementsAre(test_message_proto_.repeated_field(1)
                                   .repeated_field(0)
                                   .repeated_string(0),
                               test_message_proto_.repeated_field(1)
                                   .repeated_field(0)
                                   .repeated_string(1)),
          UnorderedElementsAre(test_message_proto_.repeated_field(1)
                                   .repeated_field(1)
                                   .repeated_string(0),
                               test_message_proto_.repeated_field(1)
                                   .repeated_field(1)
                                   .repeated_string(1)))));
}

TEST_F(RepeatedFieldExtractorTest, InvalidTypeFinderFlattened) {
  // Ensure that flattening version of field extractor can handle errors:
  auto field_info_extractor = [](const Type& type, const Field* field,
                                 CodedInputStream* input_stream)
      -> absl::StatusOr<std::vector<std::string>> {
    return absl::InvalidArgumentError("Incompatible phase of moon");
  };

  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
          "singular_field.string_field", *message_data_, field_info_extractor),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Incompatible phase of moon"));
}

TEST_F(RepeatedFieldExtractorTest,
       RepeatedGrandParentRepeatedParentRepeatedChildFlattened) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_field.repeated_field.repeated_string", *message_data_,
          GetRepeatedStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(test_message_proto_.repeated_field(0)
                                            .repeated_field(0)
                                            .repeated_string(0),
                                        test_message_proto_.repeated_field(0)
                                            .repeated_field(0)
                                            .repeated_string(1),
                                        test_message_proto_.repeated_field(0)
                                            .repeated_field(1)
                                            .repeated_string(0),
                                        test_message_proto_.repeated_field(0)
                                            .repeated_field(1)
                                            .repeated_string(1),
                                        test_message_proto_.repeated_field(1)
                                            .repeated_field(0)
                                            .repeated_string(0),
                                        test_message_proto_.repeated_field(1)
                                            .repeated_field(0)
                                            .repeated_string(1),
                                        test_message_proto_.repeated_field(1)
                                            .repeated_field(1)
                                            .repeated_string(0),
                                        test_message_proto_.repeated_field(1)
                                            .repeated_field(1)
                                            .repeated_string(1))));
}

TEST_F(RepeatedFieldExtractorTest, SingularMapSingularInt32ChildFlattened) {
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfoFlattened<int32_t>(
                  "map_singular_field.int32_field", *message_data_,
                  GetRepeatedInt32FieldExtractor()),
              IsOkAndHolds(UnorderedElementsAre(2, 22)));
}

TEST_F(RepeatedFieldExtractorTest, SingularMapSingularStringChildFlattened) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
          "map_singular_field.string_field", *message_data_,
          GetRepeatedStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre("map_singular_field_value_string_0",
                                        "map_singular_field_value_string_1")));
}

TEST_F(RepeatedFieldExtractorTest, RepeatedMapSingularStringChildFlattened) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_map_field.map_field.map_field.name", *message_data_,
          GetRepeatedStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre("1_level1_1_level2_1_level3_value",
                                        "1_level1_1_level2_2_level3_value",
                                        "1_level1_2_level2_1_level3_value",
                                        "1_level1_2_level2_2_level3_value",
                                        "2_level1_1_level2_1_level3_value",
                                        "2_level1_1_level2_2_level3_value",
                                        "2_level1_2_level2_1_level3_value",
                                        "2_level1_2_level2_2_level3_value")));
}

TEST_F(RepeatedFieldExtractorTest, RepeatedMapRepeatedStringChildFlattened) {
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_map_field.map_field.map_field.repeated_string",
          *message_data_, GetRepeatedStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          "leaf_value_01", "leaf_value_02", "leaf_value_03", "leaf_value_04",
          "leaf_value_05", "leaf_value_06", "leaf_value_07", "leaf_value_08",
          "leaf_value_09", "leaf_value_10", "leaf_value_11", "leaf_value_12",
          "leaf_value_13", "leaf_value_14", "leaf_value_15", "leaf_value_16")));
}

TEST_F(RepeatedFieldExtractorTest, MapStringValue) {
  // Extracts the values of map<string, string> within a singular parent
  // message field.
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
                  "repeated_field_leaf.map_string", *message_data_,
                  GetRepeatedStringFieldExtractor()),
              IsOkAndHolds(UnorderedElementsAre("string_0", "string_1")));
}

TEST_F(RepeatedFieldExtractorTest, RepeatedMapStringValue) {
  // Extracts the values of map<string, string> within a repeated parent
  // message
  // field.
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
                  "repeated_field.map_string", *message_data_,
                  GetRepeatedStringFieldExtractor()),
              IsOkAndHolds(UnorderedElementsAre("string_0_0", "string_0_1",
                                                "string_1_0", "string_1_1")));
}

TEST_F(RepeatedFieldExtractorTest, MalformedAnyMessage) {
  ASSERT_TRUE(test_message_proto_.mutable_singular_any_field()->PackFrom(
      test_message_proto_.singular_field()));

  auto test_message_proto = test_message_proto_;
  test_message_proto.mutable_singular_any_field()->clear_type_url();
  message_data_->Cord().Append(test_message_proto.SerializeAsCord());
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "singular_any_field.string_field", *message_data_,
          GetStringFieldExtractor()),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "Field 'singular_any_field' contains invalid google.protobuf.Any "
          "instance with empty `type_url` and non-empty `value`."));
}

TEST_F(RepeatedFieldExtractorTest, EmptyAnyMessageSkipped) {
  ASSERT_TRUE(test_message_proto_.mutable_singular_any_field()->PackFrom(
      test_message_proto_.singular_field()));
  {
    auto test_message_proto = test_message_proto_;
    test_message_proto.mutable_singular_any_field()->clear_value();
    message_data_->Cord().Append(test_message_proto.SerializeAsCord());
    EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                    "singular_any_field.string_field", *message_data_,
                    GetStringFieldExtractor()),
                IsOkAndHolds(IsEmpty()));
  }
  {
    auto test_message_proto = test_message_proto_;
    test_message_proto.mutable_singular_any_field()->clear_type_url();
    test_message_proto.mutable_singular_any_field()->clear_value();
    message_data_->Cord().Append(test_message_proto.SerializeAsCord());
    EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                    "singular_any_field.string_field", *message_data_,
                    GetStringFieldExtractor()),
                IsOkAndHolds(IsEmpty()));
  }
}

TEST_F(RepeatedFieldExtractorTest, SingularAnyField) {
  ASSERT_TRUE(test_message_proto_.mutable_singular_any_field()->PackFrom(
      test_message_proto_.singular_field()));
  message_data_->Cord().Append(test_message_proto_.SerializeAsCord());

  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "singular_any_field.string_field", *message_data_,
                  GetStringFieldExtractor()),
              IsOkAndHolds(std::vector<std::string>(
                  {test_message_proto_.singular_field().string_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<std::string>(
                  "singular_any_field.byte_field", *message_data_,
                  GetStringFieldExtractor()),
              IsOkAndHolds(std::vector<std::string>(
                  {test_message_proto_.singular_field().byte_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<TestEnum>(
                  "singular_any_field.enum_field", *message_data_,
                  GetTestEnumFieldExtractor()),
              IsOkAndHolds(std::vector<TestEnum>(
                  {test_message_proto_.singular_field().enum_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<bool>(
                  "singular_any_field.bool_field", *message_data_,
                  GetBoolFieldExtractor()),
              IsOkAndHolds(std::vector<bool>(
                  {test_message_proto_.singular_field().bool_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<double>(
                  "singular_any_field.double_field", *message_data_,
                  GetDoubleFieldExtractor()),
              IsOkAndHolds(std::vector<double>(
                  {test_message_proto_.singular_field().double_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<float>(
                  "singular_any_field.float_field", *message_data_,
                  GetFloatFieldExtractor()),
              IsOkAndHolds(std::vector<float>(
                  {test_message_proto_.singular_field().float_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint64_t>(
                  "singular_any_field.uint64_field", *message_data_,
                  GetUInt64FieldExtractor()),
              IsOkAndHolds(std::vector<uint64_t>(
                  {test_message_proto_.singular_field().uint64_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int64_t>(
                  "singular_any_field.int64_field", *message_data_,
                  GetInt64FieldExtractor()),
              IsOkAndHolds(std::vector<int64_t>(
                  {test_message_proto_.singular_field().int64_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int>(
                  "singular_any_field.int32_field", *message_data_,
                  GetInt32FieldExtractor()),
              IsOkAndHolds(std::vector<int>(
                  {test_message_proto_.singular_field().int32_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint64_t>(
                  "singular_any_field.fixed64_field", *message_data_,
                  GetFixed64FieldExtractor()),
              IsOkAndHolds(std::vector<uint64_t>(
                  {test_message_proto_.singular_field().fixed64_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint32_t>(
                  "singular_any_field.fixed32_field", *message_data_,
                  GetFixed32FieldExtractor()),
              IsOkAndHolds(std::vector<uint32_t>(
                  {test_message_proto_.singular_field().fixed32_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<uint32_t>(
                  "singular_any_field.uint32_field", *message_data_,
                  GetUInt32FieldExtractor()),
              IsOkAndHolds(std::vector<uint32_t>(
                  {test_message_proto_.singular_field().uint32_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int64_t>(
                  "singular_any_field.sfixed64_field", *message_data_,
                  GetSFixed64FieldExtractor()),
              IsOkAndHolds(std::vector<int64_t>(
                  {test_message_proto_.singular_field().sfixed64_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int32_t>(
                  "singular_any_field.sfixed32_field", *message_data_,
                  GetSFixed32FieldExtractor()),
              IsOkAndHolds(std::vector<int32_t>(
                  {test_message_proto_.singular_field().sfixed32_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int32_t>(
                  "singular_any_field.sint32_field", *message_data_,
                  GetSInt32FieldExtractor()),
              IsOkAndHolds(std::vector<int32_t>(
                  {test_message_proto_.singular_field().sint32_field()})));
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfo<int64_t>(
                  "singular_any_field.sint64_field", *message_data_,
                  GetSInt64FieldExtractor()),
              IsOkAndHolds(std::vector<int64_t>(
                  {test_message_proto_.singular_field().sint64_field()})));
}

TEST_F(RepeatedFieldExtractorTest, RepeatedAnyField) {
  for (const auto& singular_field :
       test_message_proto_.repeated_singular_fields()) {
    ASSERT_TRUE(test_message_proto_.add_repeated_any_fields()->PackFrom(
        singular_field));
  }
  message_data_->Cord().Append(test_message_proto_.SerializeAsCord());

  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "repeated_any_fields.string_field", *message_data_,
          GetStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          test_message_proto_.repeated_singular_fields(0).string_field(),
          test_message_proto_.repeated_singular_fields(1).string_field(),
          test_message_proto_.repeated_singular_fields(2).string_field())));
}

TEST_F(RepeatedFieldExtractorTest, MapAnyField) {
  int index = 0;
  for (const auto& singular_field :
       test_message_proto_.repeated_singular_fields()) {
    Any any;
    ASSERT_TRUE(any.PackFrom(singular_field));
    (*test_message_proto_
          .mutable_map_any_fields())[absl::StrCat("key-", index++)] = any;
  }
  message_data_->Cord().Append(test_message_proto_.SerializeAsCord());

  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "map_any_fields.string_field", *message_data_,
          GetStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          test_message_proto_.repeated_singular_fields(0).string_field(),
          test_message_proto_.repeated_singular_fields(1).string_field(),
          test_message_proto_.repeated_singular_fields(2).string_field())));
}

TEST_F(RepeatedFieldExtractorTest, NestedAnyField) {
  FieldExtractorTestMessage test_message_proto = test_message_proto_;
  for (const auto& singular_field :
       test_message_proto_.repeated_singular_fields()) {
    ASSERT_TRUE(
        test_message_proto.add_repeated_any_fields()->PackFrom(singular_field));
  }
  ASSERT_TRUE(test_message_proto_.mutable_singular_any_field()->PackFrom(
      test_message_proto));
  ASSERT_TRUE(test_message_proto_.add_repeated_any_fields()->PackFrom(
      test_message_proto));
  ASSERT_TRUE(test_message_proto_.add_repeated_any_fields()->PackFrom(
      test_message_proto));

  message_data_->Cord().Append(test_message_proto_.SerializeAsCord());
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "singular_any_field.repeated_any_fields.string_field", *message_data_,
          GetStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          test_message_proto_.repeated_singular_fields(0).string_field(),
          test_message_proto_.repeated_singular_fields(1).string_field(),
          test_message_proto_.repeated_singular_fields(2).string_field())));
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfo<std::string>(
          "repeated_any_fields.repeated_any_fields.string_field",
          *message_data_, GetStringFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre(
          test_message_proto_.repeated_singular_fields(0).string_field(),
          test_message_proto_.repeated_singular_fields(1).string_field(),
          test_message_proto_.repeated_singular_fields(2).string_field(),
          test_message_proto_.repeated_singular_fields(0).string_field(),
          test_message_proto_.repeated_singular_fields(1).string_field(),
          test_message_proto_.repeated_singular_fields(2).string_field())));
}

TEST_F(RepeatedFieldExtractorTest, MapStringKeyValue) {
  // Extracts the values of map<string, string> within a singular parent
  // message field.
  EXPECT_THAT(
      field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
          "repeated_field_leaf.map_string", *message_data_,
          GetRepeatedStringFieldExtractor(),
          GetRepeatedStringMapFieldExtractor()),
      IsOkAndHolds(UnorderedElementsAre("map_string_field_key_0", "string_0",
                                        "map_string_field_key_1", "string_1")));
}

TEST_F(RepeatedFieldExtractorTest, MapStringKeyValueNoMapExtractor) {
  // Extracts the values of map<string, string> within a singular parent
  // message field.
  EXPECT_THAT(field_extractor_->ExtractRepeatedFieldInfoFlattened<std::string>(
                  "repeated_field_leaf.map_string", *message_data_,
                  GetRepeatedStringFieldExtractor(), std::nullopt),
              IsOkAndHolds(UnorderedElementsAre("string_0", "string_1")));
}

}  // namespace testing
}  // namespace google::protobuf::field_extraction
