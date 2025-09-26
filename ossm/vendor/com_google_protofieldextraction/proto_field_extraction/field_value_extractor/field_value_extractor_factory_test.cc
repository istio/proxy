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

#include <functional>
#include <memory>
#include <string>

#include "google/api/service.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/testdata/field_extractor_test.pb.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "ocpdiag/core/testing/proto_matchers.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace google::protobuf::field_extraction::testing {

namespace {

using ::google::protobuf::Type;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::ocpdiag::testing::IsOkAndHolds;
using ::ocpdiag::testing::StatusIs;

// Top level of the message type url.
constexpr char kFieldExtractorTestMessageTypeUrl[] =
    "type.googleapis.com/"
    "google.protobuf.field_extraction.testing.FieldExtractorTestMessage";

}  //  namespace

class FieldValueExtractorFactoryTest : public ::testing::Test {
 protected:
  FieldValueExtractorFactoryTest() = default;

  void SetUp() override {
    ASSERT_OK(GetTextProto(
        GetTestDataFilePath("test_utils/testdata/"
                            "field_value_extractor_test_message.proto.txt"),
        &field_extractor_test_message_proto_));

    field_extractor_test_message_data_ = std::make_unique<CordMessageData>(
        field_extractor_test_message_proto_.SerializeAsCord());

    auto status = TypeHelper::Create(GetTestDataFilePath(

        "test_utils/testdata/field_extractor_test_proto_descriptor.pb"));
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());
    type_finder_ =
        absl::bind_front(&FieldValueExtractorFactoryTest::FindType, this);

    field_extractor_factory_ =
        std::make_unique<FieldValueExtractorFactory>(type_finder_);
  }

  // Tries to find the Type for `type_url`.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  std::function<const Type*(const std::string&)> type_finder_;

  testing::FieldExtractorTestMessage field_extractor_test_message_proto_;
  std::unique_ptr<CordMessageData> field_extractor_test_message_data_ = nullptr;

  std::unique_ptr<FieldValueExtractorFactory> field_extractor_factory_;
};

TEST_F(FieldValueExtractorFactoryTest, EmptyMessageType) {
  EXPECT_THAT(field_extractor_factory_->Create("", ""),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Empty message type")));
}

TEST_F(FieldValueExtractorFactoryTest, EmptyFieldPath) {
  EXPECT_THAT(field_extractor_factory_->Create("random_message_type", ""),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Empty field path")));
}

TEST_F(FieldValueExtractorFactoryTest, UnknownRootMessageType) {
  EXPECT_THAT(field_extractor_factory_->Create(
                  "type.googleapis.com/unknown_message_type",
                  "singular_field.int64_field"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Unknown root message type")));
}

using FieldValueExtractorFactorySingularLeafNodeTest =
    FieldValueExtractorFactoryTest;

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, ValidNumericField) {
  // Varint Numeric: int64, uint64, sint64, int32, uint32, sint32.
  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.int64_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.uint64_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.sint64_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.int32_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.uint32_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.sint32_field"));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest,
       ValidNonVarintNumericField) {
  // Non-varint Numeric: double, fixed64, sfixed64, float, fixed32, sfixed32.
  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.double_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.fixed64_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.sfixed64_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.float_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.fixed32_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.sfixed32_field"));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, ValidStringField) {
  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.string_field"));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, ValidTimestampField) {
  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "singular_field.timestamp_field"));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, BooleanFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field.bool_field"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, EnumFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field.enum_field"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, ByteFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field.byte_field"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

TEST_F(FieldValueExtractorFactorySingularLeafNodeTest, MessageFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

using FieldValueExtractorFactoryRepeatedLeafNodeTest =
    FieldValueExtractorFactoryTest;

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest, ValidFMapField) {
  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "repeated_field_leaf.map_string"));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest,
       ValidVarintNumericField) {
  // Varint Numeric: int64, uint64, sint64, int32, uint32, sint32.
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl, "repeated_field_leaf.repeated_int64"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_uint64"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_sint64"));

  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl, "repeated_field_leaf.repeated_int32"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_uint32"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_sint32"));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest,
       ValidNonVarintNumericField) {
  // Non-varint Numeric: double, fixed64, sfixed64, float, fixed32, sfixed32.
  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_double"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_fixed64"));

  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_field_leaf.repeated_sfixed64"));

  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl, "repeated_field_leaf.repeated_float"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_fixed32"));

  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_field_leaf.repeated_sfixed32"));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest, ValidStringField) {
  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_string"));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest, ValidTimestampField) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_field_leaf.repeated_timestamp"));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest, BooleanFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_bool"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest, EnumFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_enum"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

TEST_F(FieldValueExtractorFactoryRepeatedLeafNodeTest, ByteFieldInvalid) {
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_field_leaf.repeated_byte"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("must be numerical/string or timestamp type")));
}

TEST_F(FieldValueExtractorFactoryTest,
       ValidFieldPathNonLeafNodeAsRepeatedSingularFields) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_singular_fields.string_field"));

  EXPECT_OK(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_singular_fields.int64_field"));
}

TEST_F(FieldValueExtractorFactoryTest, ValidFieldPathAllNodesAsRepeatedFields) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_field.repeated_field.repeated_field.repeated_string"));
}

TEST_F(FieldValueExtractorFactoryTest, ValidFieldPathNonLeafNodeAsRepeatedMap) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl, "map_singular_field.string_field"));

  EXPECT_OK(field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                             "map_singular_field.int32_field"));
}

TEST_F(FieldValueExtractorFactoryTest, ValidFieldPathRepeatedNestedMap) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_map_field.map_field.map_field.name"));

  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl,
      "repeated_map_field.map_field.map_field.repeated_string"));
}

TEST_F(FieldValueExtractorFactoryTest, SingularAnyField) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl, "singular_any_field.name",
      /*support_any=*/true, /*custom_proto_map_entry_name=*/""));

  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_any_field.name"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid fieldPath (singular_any_field.name): no "
                         "'name' field")));
}

TEST_F(FieldValueExtractorFactoryTest, RepeatedAnyField) {
  EXPECT_OK(field_extractor_factory_->Create(
      kFieldExtractorTestMessageTypeUrl, "repeated_any_fields.name",
      /*support_any=*/true, /*custom_proto_map_entry_name=*/""));

  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "repeated_any_fields.name"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid fieldPath (repeated_any_fields.name): no "
                         "'name' field")));
}

TEST_F(FieldValueExtractorFactoryTest, InvalidFieldPathWithCustomMapEntryName) {
  EXPECT_THAT(
      field_extractor_factory_->Create(
          kFieldExtractorTestMessageTypeUrl, "map_singular_field.string_field",
          /*support_any=*/false,
          /*custom_proto_map_entry_name=*/"custom_map_entry"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid fieldPath")));
}

TEST_F(FieldValueExtractorFactoryTest, InvalidNonLeafNode) {
  // fixed64_field is a not a message type.
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field.fixed64_field.unknown"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid non-leaf node fixed64_field")));
}

TEST_F(FieldValueExtractorFactoryTest, InvalidFieldPathFieldNotExist) {
  // Normal Case.
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field.not_exist"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid fieldPath (singular_field.not_exist): no "
                         "'not_exist' field")));
  // Map Case.
  EXPECT_THAT(
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "map_singular_field.not_exist"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid fieldPath (map_singular_field.not_exist): no "
                         "'not_exist' field")));
}

using CreateFieldValueExtractorFactoryTest = FieldValueExtractorFactoryTest;

TEST_F(CreateFieldValueExtractorFactoryTest, CreateAndExtractSingularString) {
  ASSERT_OK_AND_ASSIGN(
      auto field_extractor,
      field_extractor_factory_->Create(kFieldExtractorTestMessageTypeUrl,
                                       "singular_field.string_field"));

  EXPECT_THAT(field_extractor->Extract(*field_extractor_test_message_data_),
              IsOkAndHolds(ElementsAre(
                  field_extractor_test_message_proto_.singular_field()
                      .string_field())));
}

}  // namespace google::protobuf::field_extraction::testing
