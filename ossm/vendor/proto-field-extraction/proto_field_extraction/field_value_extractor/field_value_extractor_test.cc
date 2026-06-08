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
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "google/api/service.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "ocpdiag/core/testing/parse_text_proto.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "grpc_transcoding/type_helper.h"
#include "proto_field_extraction/field_extractor/field_extractor.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/testdata/field_extractor_test.pb.h"
#include "proto_field_extraction/test_utils/utils.h"

#include "ocpdiag/core/testing/proto_matchers.h"
#include "ocpdiag/core/testing/status_matchers.h"
#include "google/protobuf/map.h"

namespace google::protobuf::field_extraction {
namespace testing {

namespace {

using ::google::protobuf::Type;
using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::ElementsAre;
using ::ocpdiag::testing::EqualsProto;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using ::testing::proto::IgnoringRepeatedFieldOrdering;
using ::ocpdiag::testing::IsOkAndHolds;
using ::ocpdiag::testing::ParseTextProtoOrDie;

// Top level of the message type url.
constexpr char kSingularFieldTestMessageTypeUrl[] =
    "type.googleapis.com/"
    "google.protobuf.field_extraction.testing.SingularFieldTestMessage";

// Top level of the message type url.
constexpr char kFieldExtractorTestMessageTypeUrl[] =
    "type.googleapis.com/"
    "google.protobuf.field_extraction.testing.FieldExtractorTestMessage";

}  // namespace

class FieldValueExtractorTest : public ::testing::Test {
 protected:
  FieldValueExtractorTest() = default;

  void SetUp() override {
    ASSERT_OK(GetTextProto(
        GetTestDataFilePath("test_utils/testdata/"
                            "field_value_extractor_test_message.proto.txt"),
        &field_extractor_test_message_proto_));
    singular_field_test_message_proto_ =
        field_extractor_test_message_proto_.singular_field();

    field_extractor_ = std::make_unique<CordMessageData>(
        field_extractor_test_message_proto_.SerializeAsCord());
    singular_field_ = std::make_unique<CordMessageData>(
        singular_field_test_message_proto_.SerializeAsCord());

    auto status = TypeHelper::Create(GetTestDataFilePath(
        "test_utils/testdata/field_extractor_test_proto_descriptor.pb"));
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());
    type_finder_ = absl::bind_front(&FieldValueExtractorTest::FindType, this);

    field_extractor_test_message_type_ =
        type_finder_(kFieldExtractorTestMessageTypeUrl);
    ASSERT_NE(field_extractor_test_message_type_, nullptr);
    ASSERT_NE(field_extractor_test_message_type_, nullptr);
    singular_field_test_message_type_ =
        type_finder_(kSingularFieldTestMessageTypeUrl);
    ASSERT_NE(singular_field_test_message_type_, nullptr);
    ASSERT_NE(singular_field_test_message_type_, nullptr);
  }

  // Tries to find the Type for `type_url`.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  CreateFieldExtractorFunc GetCreateFieldExtractorFunc(const Type& type) {
    return [this, &type]() {
      return std::make_unique<
          google::protobuf::field_extraction::FieldExtractor>(&type,
                                                              type_finder_);
    };
  }

  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  std::function<const Type*(const std::string&)> type_finder_;

  const Type* field_extractor_test_message_type_;
  testing::FieldExtractorTestMessage field_extractor_test_message_proto_;

  const Type* singular_field_test_message_type_;
  testing::SingularFieldTestMessage singular_field_test_message_proto_;
  std::unique_ptr<CordMessageData> field_extractor_ = nullptr;
  std::unique_ptr<CordMessageData> singular_field_ = nullptr;
};

using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ExtractSingularFieldTest = FieldValueExtractorTest;

TEST_F(ExtractSingularFieldTest, TypeString) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"string_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(
                  singular_field_test_message_proto_.string_field())));
}

TEST_F(ExtractSingularFieldTest, TypeInt64) {
  {
    // Type: int64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"int64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.int64_field()))));
  }
  {
    // Type: uint64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"uint64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.uint64_field()))));
  }
  {
    // Type: sint64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sint64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.sint64_field()))));
  }
}

TEST_F(ExtractSingularFieldTest, TypeInt32) {
  {
    // Type: int32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"int32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.int32_field()))));
  }
  {
    // Type: uint32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"uint32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.uint32_field()))));
  }
  {
    // Type: sint32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sint32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.sint32_field()))));
  }
}

TEST_F(ExtractSingularFieldTest, TypeFloat) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"float_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(absl::StrCat(
                  singular_field_test_message_proto_.float_field()))));
}

TEST_F(ExtractSingularFieldTest, TypeDouble) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"double_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(absl::StrCat(
                  singular_field_test_message_proto_.double_field()))));
}

TEST_F(ExtractSingularFieldTest, TypeFixedInt) {
  {
    // Type: fixed 32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"fixed32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.fixed32_field()))));
  }
  {
    // Type: fixed 64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"fixed64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.fixed64_field()))));
  }
  {
    // Type: sfixed 32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sfixed32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.sfixed32_field()))));
  }
  {
    // Type: sfixed 64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sfixed64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    singular_field_test_message_proto_.sfixed64_field()))));
  }
}

TEST_F(ExtractSingularFieldTest, TypeTimestamp) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"timestamp_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
  // Field value extractor supports extracting Timestamp as a serialized string.
  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(
                  singular_field_test_message_proto_.timestamp_field()
                      .SerializeAsString())));
}

using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ExtractSingularFieldHasDuplicateTest = FieldValueExtractorTest;

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeString) {
  std::string last_string = "boom!";
  testing::SingularFieldTestMessage append_request;
  append_request.set_string_field(last_string);
  singular_field_->Cord().Append(append_request.SerializeAsCord());

  FieldValueExtractor field_extractor(
      /*field_path=*/"string_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(last_string)));
}

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeInt64) {
  int64_t last_int64 = 66;
  uint64_t last_uint64 = 321;
  int64_t last_sint64 = 12378978900;

  testing::SingularFieldTestMessage append_request;
  append_request.set_int64_field(last_int64);
  append_request.set_uint64_field(last_uint64);
  append_request.set_sint64_field(last_sint64);
  singular_field_->Cord().Append(append_request.SerializeAsCord());
  {
    // Type: int64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"int64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_int64))));
  }
  {
    // Type: uint64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"uint64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_uint64))));
  }
  {
    // Type: sint64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sint64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_sint64))));
  }
}

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeInt32) {
  int32_t last_int32 = 4321;
  uint32_t last_uint32 = 3214567;
  int32_t last_sint32 = 1237897890;

  testing::SingularFieldTestMessage append_request;
  append_request.set_int32_field(last_int32);
  append_request.set_uint32_field(last_uint32);
  append_request.set_sint32_field(last_sint32);
  singular_field_->Cord().Append(append_request.SerializeAsCord());
  singular_field_->Cord().Append(append_request.SerializeAsCord());
  {
    // Type: int32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"int32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_int32))));
  }
  {
    // Type: uint32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"uint32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_uint32))));
  }
  {
    // Type: sint32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sint32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_sint32))));
  }
}

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeFloat) {
  float last_float = 6.66;
  testing::SingularFieldTestMessage append_request;
  append_request.set_float_field(last_float);
  singular_field_->Cord().Append(append_request.SerializeAsCord());
  FieldValueExtractor field_extractor(
      /*field_path=*/"float_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(absl::StrCat(last_float))));
}

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeDouble) {
  double last_double = 6.666;
  testing::SingularFieldTestMessage append_request;
  append_request.set_double_field(last_double);
  singular_field_->Cord().Append(append_request.SerializeAsCord());
  FieldValueExtractor field_extractor(
      /*field_path=*/"double_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(absl::StrCat(last_double))));
}

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeFixedInt) {
  uint32_t last_fixed32 = 125436;
  uint64_t last_fixed64 = 12545;
  int32_t last_sfixed32 = 123789789;
  int64_t last_sfixed64 = 12378978;
  testing::SingularFieldTestMessage append_request;
  append_request.set_fixed32_field(last_fixed32);
  append_request.set_fixed64_field(last_fixed64);
  append_request.set_sfixed32_field(last_sfixed32);
  append_request.set_sfixed64_field(last_sfixed64);
  singular_field_->Cord().Append(append_request.SerializeAsCord());

  singular_field_->Cord().Append(append_request.SerializeAsCord());

  {
    // Type: fixed 32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"fixed32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_fixed32))));
  }
  {
    // Type: fixed 64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"fixed64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_fixed64))));
  }
  {
    // Type: sfixed 32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sfixed32_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_sfixed32))));
  }
  {
    // Type: sfixed 64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"sfixed64_field",
        GetCreateFieldExtractorFunc(*singular_field_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*singular_field_),
                IsOkAndHolds(ElementsAre(absl::StrCat(last_sfixed64))));
  }
}

TEST_F(ExtractSingularFieldHasDuplicateTest, TypeTimestamp) {
  google::protobuf::Timestamp last_timestamp =
      ParseTextProtoOrDie(R"pb(seconds: 1237897890, nanos: 5)pb");

  testing::SingularFieldTestMessage append_request;
  *append_request.mutable_timestamp_field() = last_timestamp;
  singular_field_->Cord().Append(append_request.SerializeAsCord());
  singular_field_->Cord().Append(append_request.SerializeAsCord());

  FieldValueExtractor field_extractor(
      /*field_path=*/"timestamp_field",
      GetCreateFieldExtractorFunc(*singular_field_test_message_type_));
  // FieldValueExtractor supports extracting Timestamp as a serialized string.
  EXPECT_THAT(field_extractor.Extract(*singular_field_),
              IsOkAndHolds(ElementsAre(last_timestamp.SerializeAsString())));
}

using ExtractSingularFieldLeafNode = FieldValueExtractorTest;

TEST_F(ExtractSingularFieldLeafNode, TypeString) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"singular_field.string_field",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(ElementsAre(
                  field_extractor_test_message_proto_.singular_field()
                      .string_field())));
}

TEST_F(ExtractSingularFieldLeafNode, TypeInt64) {
  {
    // Type: int64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.int64_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .int64_field()))));
  }
  {
    // Type: uint64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.uint64_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .uint64_field()))));
  }
  {
    // Type: sint64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.sint64_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .sint64_field()))));
  }
}

TEST_F(ExtractSingularFieldLeafNode, TypeInt32) {
  {
    // Type: int32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.int32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .int32_field()))));
  }
  {
    // Type: uint32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.uint32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .uint32_field()))));
  }
  {
    // Type: sint32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.sint32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .sint32_field()))));
  }
}

TEST_F(ExtractSingularFieldLeafNode, TypeFloat) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"singular_field.float_field",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(ElementsAre(absl::StrCat(
                  field_extractor_test_message_proto_.singular_field()
                      .float_field()))));
}

TEST_F(ExtractSingularFieldLeafNode, TypeDouble) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"singular_field.double_field",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(ElementsAre(absl::StrCat(
                  field_extractor_test_message_proto_.singular_field()
                      .double_field()))));
}

TEST_F(ExtractSingularFieldLeafNode, TypeFixedInt) {
  {
    // Type: fixed32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.fixed32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .fixed32_field()))));
  }
  {
    // Type: fixed64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.fixed64_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .fixed64_field()))));
  }
  {
    // Type: sfixed32.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.sfixed32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .sfixed32_field()))));
  }
  {
    // Type: sfixed64.
    FieldValueExtractor field_extractor(
        /*field_path=*/"singular_field.sfixed64_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(ElementsAre(absl::StrCat(
                    field_extractor_test_message_proto_.singular_field()
                        .sfixed64_field()))));
  }
}

TEST_F(ExtractSingularFieldLeafNode, TypeTimestamp) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"singular_field.timestamp_field",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  // FieldValueExtractor supports extracting Timestamp as a serialized string.
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(ElementsAre(
                  field_extractor_test_message_proto_.singular_field()
                      .timestamp_field()
                      .SerializeAsString())));
}

using ExtractRepeatedFieldLeafNode = FieldValueExtractorTest;

TEST_F(ExtractRepeatedFieldLeafNode, TypeString) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field_leaf.repeated_string",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(UnorderedElementsAre(
                  field_extractor_test_message_proto_.repeated_field_leaf()
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field_leaf()
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field_leaf()
                      .repeated_string(2),
                  field_extractor_test_message_proto_.repeated_field_leaf()
                      .repeated_string(3))));
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeTimestamp) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field_leaf.repeated_timestamp",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(UnorderedElementsAre(
                  field_extractor_test_message_proto_.repeated_field_leaf()
                      .repeated_timestamp(0)
                      .SerializeAsString(),
                  field_extractor_test_message_proto_.repeated_field_leaf()
                      .repeated_timestamp(1)
                      .SerializeAsString())));
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeInt64) {
  {
    // Pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_int64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_int64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_int64(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_int64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, int64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_int64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf non string key
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_int64_int64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<int64_t, int64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_int64_int64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_int64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_int64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_int64(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeUnsignedInt64) {
  {
    // Pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_uint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_uint64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_uint64(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_uint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, uint64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_uint64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_uint64_uint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<uint64_t, uint64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_uint64_uint64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_uint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_uint64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_uint64(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeSignedInt64) {
  {
    // Pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_sint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sint64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sint64(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, int64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_sint64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sint64_sint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<int64_t, int64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_sint64_sint64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_sint64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sint64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sint64(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeInt32) {
  {
    // Pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_int32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_int32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_int32(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_int32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, int32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_int32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_int32_int32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<int32_t, int32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_int32_int32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_int32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_int32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_int32(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeUnsignedInt32) {
  {
    // Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_uint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_uint32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_uint32(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_uint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, uint32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_uint32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_uint32_uint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<uint32_t, uint32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_uint32_uint32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_uint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_uint32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_uint32(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeSignedInt32) {
  {
    // Pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_sint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sint32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sint32(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, int32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_sint32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sint32_sint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<int32_t, int32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_sint32_sint32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_sint32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sint32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sint32(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeFloat) {
  {
    // Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_float",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_float(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_float(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_float",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, float>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_float();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_float",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_float(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_float(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeDouble) {
  {
    // Pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_double",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_double(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_double(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_double",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, double>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_double();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_double",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_double(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_double(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeFixed64) {
  {
    // Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_fixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_fixed64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_fixed64(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_fixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, uint64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_fixed64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_fixed64_fixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<uint64_t, uint64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_fixed64_fixed64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_fixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_fixed64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_fixed64(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeSignedFixed64) {
  {
    // Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_sfixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sfixed64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sfixed64(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sfixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, int64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_sfixed64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sfixed64_sfixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<int64_t, int64_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_sfixed64_sfixed64();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_sfixed64",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sfixed64(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sfixed64(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeFixed32) {
  {
    // Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_fixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_fixed32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_fixed32(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_fixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, uint32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf().map_fixed32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_fixed32_fixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<uint32_t, uint32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_fixed32_fixed32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_fixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_fixed32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_fixed32(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, TypeSignedFixed32) {
  {
    // Pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.repeated_sfixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sfixed32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf()
                                     .repeated_sfixed32(1)))));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sfixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<std::string, int32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_sfixed32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Map leaf
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf.map_sfixed32_sfixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    std::vector<std::string> expected_value;
    const google::protobuf::Map<int32_t, int32_t>& map_entry =
        field_extractor_test_message_proto_.repeated_field_leaf()
            .map_sfixed32_sfixed32();
    expected_value.reserve(map_entry.size());
    for (const auto& map_entry : map_entry) {
      expected_value.push_back(absl::StrCat(map_entry.second));
    }

    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
  }
  {
    // Non-pack encoding.
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_field_leaf_unpack.repeated_sfixed32",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sfixed32(0)),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_field_leaf_unpack()
                                     .repeated_sfixed32(1)))));
  }
}

TEST_F(ExtractRepeatedFieldLeafNode, Bool) {
  // Map leaf
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field_leaf.map_bool_bool",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

  // Bool value is not supported by field extractor.
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              ::ocpdiag::testing::StatusIs(
                  absl::StatusCode::kInternal,
                  "Unexpected field type for repeated primitive field: value"));
}

TEST_F(FieldValueExtractorTest, ExtractNonLeafNodeAsRepeatedSingularFields) {
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_singular_fields.string_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(
        field_extractor.Extract(*field_extractor_),
        IsOkAndHolds(UnorderedElementsAre(
            field_extractor_test_message_proto_.repeated_singular_fields(0)
                .string_field(),
            field_extractor_test_message_proto_.repeated_singular_fields(1)
                .string_field(),
            field_extractor_test_message_proto_.repeated_singular_fields(2)
                .string_field())));
  }
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_singular_fields.int64_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre(
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_singular_fields(0)
                                     .int64_field()),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_singular_fields(1)
                                     .int64_field()),
                    absl::StrCat(field_extractor_test_message_proto_
                                     .repeated_singular_fields(2)
                                     .int64_field()))));
  }
}

TEST_F(FieldValueExtractorTest, ExtractAllNodesAsRepeatedFields) {
  FieldValueExtractor field_extractor(
      /*field_path=*/
      "repeated_field.repeated_field.repeated_field.repeated_string",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(UnorderedElementsAre(
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(0)
                      .repeated_field(0)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(0)
                      .repeated_field(0)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(0)
                      .repeated_field(1)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(0)
                      .repeated_field(1)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(1)
                      .repeated_field(0)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(1)
                      .repeated_field(0)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(1)
                      .repeated_field(1)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(0)
                      .repeated_field(1)
                      .repeated_field(1)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(0)
                      .repeated_field(0)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(0)
                      .repeated_field(0)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(0)
                      .repeated_field(1)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(0)
                      .repeated_field(1)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(1)
                      .repeated_field(0)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(1)
                      .repeated_field(0)
                      .repeated_string(1),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(1)
                      .repeated_field(1)
                      .repeated_string(0),
                  field_extractor_test_message_proto_.repeated_field(1)
                      .repeated_field(1)
                      .repeated_field(1)
                      .repeated_string(1))));
}

using ExtractMapFieldTest = FieldValueExtractorTest;

TEST_F(ExtractMapFieldTest, LeafNodeTypeString) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field_leaf.map_string",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  std::vector<std::string> expected_value;
  const google::protobuf::Map<std::string, std::string>& map_string =
      field_extractor_test_message_proto_.repeated_field_leaf().map_string();
  expected_value.reserve(map_string.size());
  for (const auto& map_entry : map_string) {
    expected_value.push_back(map_entry.second);
  }

  EXPECT_THAT(field_extractor.Extract(*field_extractor_),
              IsOkAndHolds(UnorderedElementsAreArray(expected_value)));
}

TEST_F(ExtractMapFieldTest, AllMapValueInRepeatedFields) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field.repeated_field.repeated_field.map_string",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
  EXPECT_THAT(
      field_extractor.Extract(*field_extractor_),
      IsOkAndHolds(UnorderedElementsAre("1_level1_1_level2_1_leaf_string_0",
                                        "1_level1_1_level2_1_leaf_string_1",
                                        "1_level1_1_level2_2_leaf_string_0",
                                        "1_level1_1_level2_2_leaf_string_1",
                                        "1_level1_2_level2_1_leaf_string_0",
                                        "1_level1_2_level2_1_leaf_string_1",
                                        "1_level1_2_level2_2_leaf_string_0",
                                        "1_level1_2_level2_2_leaf_string_1")));
}

TEST_F(ExtractMapFieldTest, NonLeafNodeAsRepeatedMap) {
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"map_singular_field.string_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(
                    UnorderedElementsAre("map_singular_field_value_string_0",
                                         "map_singular_field_value_string_1")));
  }
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"map_singular_field.int32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(field_extractor.Extract(*field_extractor_),
                IsOkAndHolds(UnorderedElementsAre("2", "22")));
  }
}

TEST_F(ExtractMapFieldTest, RepeatedNestedMap) {
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_map_field.map_field.map_field.name",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(
        field_extractor.Extract(*field_extractor_),
        IsOkAndHolds(UnorderedElementsAre("1_level1_1_level2_1_level3_value",
                                          "1_level1_1_level2_2_level3_value",
                                          "1_level1_2_level2_1_level3_value",
                                          "1_level1_2_level2_2_level3_value",
                                          "2_level1_1_level2_1_level3_value",
                                          "2_level1_1_level2_2_level3_value",
                                          "2_level1_2_level2_1_level3_value",
                                          "2_level1_2_level2_2_level3_value")));
  }
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"repeated_map_field.map_field.map_field.repeated_string",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));
    EXPECT_THAT(
        field_extractor.Extract(*field_extractor_),
        IsOkAndHolds(UnorderedElementsAre(
            "leaf_value_01", "leaf_value_02", "leaf_value_03", "leaf_value_04",
            "leaf_value_05", "leaf_value_06", "leaf_value_07", "leaf_value_08",
            "leaf_value_09", "leaf_value_10", "leaf_value_11", "leaf_value_12",
            "leaf_value_13", "leaf_value_14", "leaf_value_15",
            "leaf_value_16")));
  }
}

using ExtractFieldAsProtoValueTest = FieldValueExtractorTest;

TEST_F(ExtractFieldAsProtoValueTest, LeafNodeTypeString) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field_leaf.map_string",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

  EXPECT_THAT(field_extractor.ExtractValue(*field_extractor_),
              IsOkAndHolds(IgnoringRepeatedFieldOrdering(EqualsProto(
                  R"pb(
                    list_value {
                      values {
                        struct_value {
                          fields {
                            key: "map_string_field_key_0"
                            value { string_value: "string_0" }
                          }
                          fields {
                            key: "map_string_field_key_1"
                            value { string_value: "string_1" }
                          }
                        }
                      }
                    }
                  )pb"))));
}

TEST_F(ExtractFieldAsProtoValueTest, AllMapValueInRepeatedFields) {
  FieldValueExtractor field_extractor(
      /*field_path=*/"repeated_field.repeated_field.repeated_field.map_string",
      GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

  EXPECT_THAT(
      field_extractor.ExtractValue(*field_extractor_),
      IsOkAndHolds(IgnoringRepeatedFieldOrdering(EqualsProto(
          R"pb(
            list_value {
              values {
                struct_value {
                  fields {
                    key: "map_string_field_key_0"
                    value { string_value: "1_level1_1_level2_1_leaf_string_0" }
                  }
                  fields {
                    key: "map_string_field_key_1"
                    value { string_value: "1_level1_1_level2_1_leaf_string_1" }
                  }
                }
              }
              values {
                struct_value {
                  fields {
                    key: "map_string_field_key_0"
                    value { string_value: "1_level1_1_level2_2_leaf_string_0" }
                  }
                  fields {
                    key: "map_string_field_key_1"
                    value { string_value: "1_level1_1_level2_2_leaf_string_1" }
                  }
                }
              }
              values {
                struct_value {
                  fields {
                    key: "map_string_field_key_0"
                    value { string_value: "1_level1_2_level2_1_leaf_string_0" }
                  }
                  fields {
                    key: "map_string_field_key_1"
                    value { string_value: "1_level1_2_level2_1_leaf_string_1" }
                  }
                }
              }
              values {
                struct_value {
                  fields {
                    key: "map_string_field_key_0"
                    value { string_value: "1_level1_2_level2_2_leaf_string_0" }
                  }
                  fields {
                    key: "map_string_field_key_1"
                    value { string_value: "1_level1_2_level2_2_leaf_string_1" }
                  }
                }
              }
            }
          )pb"))));
}

TEST_F(ExtractFieldAsProtoValueTest, NonLeafNodeAsRepeatedMap) {
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"map_singular_field.string_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    EXPECT_THAT(
        field_extractor.ExtractValue(*field_extractor_),
        IsOkAndHolds(IgnoringRepeatedFieldOrdering(EqualsProto(
            R"pb(
              list_value {
                values { string_value: "map_singular_field_value_string_0" }
                values { string_value: "map_singular_field_value_string_1" }
              }
            )pb"))));
  }
  {
    FieldValueExtractor field_extractor(
        /*field_path=*/"map_singular_field.int32_field",
        GetCreateFieldExtractorFunc(*field_extractor_test_message_type_));

    EXPECT_THAT(field_extractor.ExtractValue(*field_extractor_),
                IsOkAndHolds(IgnoringRepeatedFieldOrdering(EqualsProto(
                    R"pb(
                      list_value {
                        values { string_value: "2" }
                        values { string_value: "22" }
                      }
                    )pb"))));
  }
}
}  // namespace testing
}  // namespace google::protobuf::field_extraction
