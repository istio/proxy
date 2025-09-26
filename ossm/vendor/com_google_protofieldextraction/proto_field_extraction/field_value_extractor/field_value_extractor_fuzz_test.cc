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

#include "google/api/service.pb.h"
#include "gmock/gmock.h"
#include "testing/fuzzing/fuzztest.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/field_value_extractor/field_value_extractor.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/testdata/field_extractor_test.pb.h"
#include "proto_field_extraction/test_utils/utils.h"

namespace google::protobuf::field_extraction::testing {
namespace {

using ::google::protobuf::Type;

// Top level of the message type url.
constexpr char kFieldExtractorTestMessageTypeUrl[] =
    "type.googleapis.com/"
    "google.protobuf.field_extraction.testing.FieldExtractorTestMessage";

}  // namespace
class FieldValueExtractorFuzzer {
 public:
  explicit FieldValueExtractorFuzzer() { SetUp(); }

  void SetUp() {
    auto status = TypeHelper::Create(GetTestDataFilePath(
        "test_utils/testdata/field_extractor_test_proto_descriptor.pb"));
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());
    type_finder_ = absl::bind_front(&FieldValueExtractorFuzzer::FindType, this);

    field_extractor_test_message_type_ =
        type_finder_(kFieldExtractorTestMessageTypeUrl);
    field_extractor_builder_ = std::make_unique<CordMessageData>(
        field_extractor_test_message_proto_.SerializeAsCord());
  }

  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  CreateFieldExtractorFunc GetCreateFieldExtractorFunc() {
    return [this]() {
      return std::make_unique<
          google::protobuf::field_extraction::FieldExtractor>(
          field_extractor_test_message_type_, type_finder_);
    };
  }

  std::unique_ptr<TypeHelper> type_helper_;
  std::function<const Type*(const std::string&)> type_finder_;

  const Type* field_extractor_test_message_type_;
  testing::FieldExtractorTestMessage field_extractor_test_message_proto_;
  std::unique_ptr<CordMessageData> field_extractor_builder_ = nullptr;
};

FieldValueExtractorFuzzer& fuzzer_init() {
  static auto* fuzzer = new FieldValueExtractorFuzzer();
  return *fuzzer;
}

void ExtractFields(FieldExtractorTestMessage test_message) {
  FieldValueExtractor field_extractor(
      "singular_field.string_field",
      fuzzer_init().GetCreateFieldExtractorFunc());

  EXPECT_OK(field_extractor.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_1(
      "singular_field.uint64_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_1.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_2(
      "singular_field.sint64_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_2.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_3(
      "singular_field.int32_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_3.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_4(
      "singular_field.uint32_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_4.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_5(
      "singular_field.sint32_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_5.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_6(
      "singular_field.float_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_6.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_7(
      "singular_field.double_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_7.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_8(
      "singular_field.fixed64_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_8.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_9(
      "singular_field.sfixed32_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(field_extractor_9.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_10(
      "singular_field.sfixed64_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_10.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_11(
      "singular_field.timestamp_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_11.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_12(
      "repeated_field_leaf.repeated_string",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_12.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_13(
      "repeated_field_leaf.repeated_timestamp",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_13.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_14(
      "repeated_field_leaf.repeated_int64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_14.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_15(
      "repeated_field_leaf_unpack.repeated_int64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_15.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_16(
      "repeated_field_leaf.repeated_uint64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_16.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_17(
      "repeated_field_leaf_unpack.repeated_uint64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_17.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_18(
      "repeated_field_leaf.repeated_sint64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_18.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_19(
      "repeated_field_leaf_unpack.repeated_sint64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_19.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_20(
      "repeated_field_leaf.repeated_int32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_20.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_21(
      "repeated_field_leaf_unpack.repeated_int32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_21.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_22(
      "repeated_field_leaf.repeated_uint32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_22.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_23(
      "repeated_field_leaf_unpack.repeated_uint32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_23.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_24(
      "repeated_field_leaf.repeated_sint32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_24.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_25(
      "repeated_field_leaf_unpack.repeated_sint32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_25.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_26(
      "repeated_field_leaf.repeated_float",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_26.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_27(
      "repeated_field_leaf_unpack.repeated_float",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_27.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_28(
      "repeated_field_leaf.repeated_double",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_28.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_29(
      "repeated_field_leaf_unpack.repeated_double",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_29.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_30(
      "repeated_field_leaf.repeated_fixed64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_30.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_31(
      "repeated_field_leaf_unpack.repeated_fixed64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_31.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_32(
      "repeated_field_leaf.repeated_sfixed64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_32.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_33(
      "repeated_field_leaf_unpack.repeated_sfixed64",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_33.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_34(
      "repeated_field_leaf.repeated_fixed32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_34.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_35(
      "repeated_field_leaf_unpack.repeated_fixed32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_35.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_36(
      "repeated_field_leaf.repeated_sfixed32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_36.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_37(
      "repeated_field_leaf_unpack.repeated_sfixed32",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_37.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_38(
      "repeated_singular_fields.string_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_38.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_39(
      "repeated_singular_fields.int64_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_39.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_40(
      "repeated_field.repeated_field.repeated_field.repeated_string",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_40.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_41(
      "repeated_field_leaf.map_string",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_41.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_42(
      "repeated_field.repeated_field.repeated_field.map_string",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_42.Extract(*fuzzer_init().field_extractor_builder_));
  FieldValueExtractor field_extractor_43(
      "map_singular_field.string_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_43.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_44(
      "map_singular_field.int32_field",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_44.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_45(
      "repeated_map_field.map_field.map_field.name",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_45.Extract(*fuzzer_init().field_extractor_builder_));

  FieldValueExtractor field_extractor_46(
      "repeated_map_field.map_field.map_field.repeated_string",
      fuzzer_init().GetCreateFieldExtractorFunc());
  EXPECT_OK(
      field_extractor_46.Extract(*fuzzer_init().field_extractor_builder_));
}
FUZZ_TEST(FieldValueExtractorFuzzTests, ExtractFields)
    .WithDomains(fuzztest::Arbitrary<FieldExtractorTestMessage>());
}  // namespace google::protobuf::field_extraction::testing
