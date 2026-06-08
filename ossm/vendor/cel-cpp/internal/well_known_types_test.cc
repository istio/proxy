// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/well_known_types.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/memory.h"
#include "internal/message_type_name.h"
#include "internal/minimal_descriptor_pool.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::well_known_types {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::internal::GetMinimalDescriptorPool;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Test;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

class ReflectionTest : public Test {
 public:
  google::protobuf::Arena* absl_nonnull arena() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return &arena_;
  }

  std::string& scratch_space() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return scratch_space_;
  }

  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  google::protobuf::MessageFactory* absl_nonnull message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  T* absl_nonnull MakeGenerated() {
    return google::protobuf::Arena::Create<T>(arena());
  }

  template <typename T>
  google::protobuf::Message* absl_nonnull MakeDynamic() {
    const auto* descriptor =
        ABSL_DIE_IF_NULL(descriptor_pool()->FindMessageTypeByName(
            internal::MessageTypeNameFor<T>()));
    const auto* prototype =
        ABSL_DIE_IF_NULL(message_factory()->GetPrototype(descriptor));
    return prototype->New(arena());
  }

 private:
  google::protobuf::Arena arena_;
  std::string scratch_space_;
};

TEST_F(ReflectionTest, MinimalDescriptorPool) {
  EXPECT_THAT(Reflection().Initialize(GetMinimalDescriptorPool()), IsOk());
}

TEST_F(ReflectionTest, TestingDescriptorPool) {
  EXPECT_THAT(Reflection().Initialize(GetTestingDescriptorPool()), IsOk());
}

TEST_F(ReflectionTest, BoolValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::BoolValue>();
  EXPECT_EQ(BoolValueReflection::GetValue(*value), false);
  BoolValueReflection::SetValue(value, true);
  EXPECT_EQ(BoolValueReflection::GetValue(*value), true);
}

TEST_F(ReflectionTest, BoolValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::BoolValue>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetBoolValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), false);
  reflection.SetValue(value, true);
  EXPECT_EQ(reflection.GetValue(*value), true);
}

TEST_F(ReflectionTest, Int32Value_Generated) {
  auto* value = MakeGenerated<google::protobuf::Int32Value>();
  EXPECT_EQ(Int32ValueReflection::GetValue(*value), 0);
  Int32ValueReflection::SetValue(value, 1);
  EXPECT_EQ(Int32ValueReflection::GetValue(*value), 1);
}

TEST_F(ReflectionTest, Int32Value_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Int32Value>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetInt32ValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), 0);
  reflection.SetValue(value, 1);
  EXPECT_EQ(reflection.GetValue(*value), 1);
}

TEST_F(ReflectionTest, Int64Value_Generated) {
  auto* value = MakeGenerated<google::protobuf::Int64Value>();
  EXPECT_EQ(Int64ValueReflection::GetValue(*value), 0);
  Int64ValueReflection::SetValue(value, 1);
  EXPECT_EQ(Int64ValueReflection::GetValue(*value), 1);
}

TEST_F(ReflectionTest, Int64Value_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Int64Value>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetInt64ValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), 0);
  reflection.SetValue(value, 1);
  EXPECT_EQ(reflection.GetValue(*value), 1);
}

TEST_F(ReflectionTest, UInt32Value_Generated) {
  auto* value = MakeGenerated<google::protobuf::UInt32Value>();
  EXPECT_EQ(UInt32ValueReflection::GetValue(*value), 0);
  UInt32ValueReflection::SetValue(value, 1);
  EXPECT_EQ(UInt32ValueReflection::GetValue(*value), 1);
}

TEST_F(ReflectionTest, UInt32Value_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::UInt32Value>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetUInt32ValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), 0);
  reflection.SetValue(value, 1);
  EXPECT_EQ(reflection.GetValue(*value), 1);
}

TEST_F(ReflectionTest, UInt64Value_Generated) {
  auto* value = MakeGenerated<google::protobuf::UInt64Value>();
  EXPECT_EQ(UInt64ValueReflection::GetValue(*value), 0);
  UInt64ValueReflection::SetValue(value, 1);
  EXPECT_EQ(UInt64ValueReflection::GetValue(*value), 1);
}

TEST_F(ReflectionTest, UInt64Value_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::UInt64Value>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetUInt64ValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), 0);
  reflection.SetValue(value, 1);
  EXPECT_EQ(reflection.GetValue(*value), 1);
}

TEST_F(ReflectionTest, FloatValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::FloatValue>();
  EXPECT_EQ(FloatValueReflection::GetValue(*value), 0);
  FloatValueReflection::SetValue(value, 1);
  EXPECT_EQ(FloatValueReflection::GetValue(*value), 1);
}

TEST_F(ReflectionTest, FloatValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::FloatValue>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetFloatValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), 0);
  reflection.SetValue(value, 1);
  EXPECT_EQ(reflection.GetValue(*value), 1);
}

TEST_F(ReflectionTest, DoubleValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::DoubleValue>();
  EXPECT_EQ(DoubleValueReflection::GetValue(*value), 0);
  DoubleValueReflection::SetValue(value, 1);
  EXPECT_EQ(DoubleValueReflection::GetValue(*value), 1);
}

TEST_F(ReflectionTest, DoubleValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::DoubleValue>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetDoubleValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value), 0);
  reflection.SetValue(value, 1);
  EXPECT_EQ(reflection.GetValue(*value), 1);
}

TEST_F(ReflectionTest, BytesValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::BytesValue>();
  EXPECT_EQ(BytesValueReflection::GetValue(*value), "");
  BytesValueReflection::SetValue(value, absl::Cord("Hello World!"));
  EXPECT_EQ(BytesValueReflection::GetValue(*value), "Hello World!");
}

TEST_F(ReflectionTest, BytesValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::BytesValue>();
  std::string scratch;
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetBytesValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value, scratch), "");
  reflection.SetValue(value, "Hello World!");
  EXPECT_EQ(reflection.GetValue(*value, scratch), "Hello World!");
  reflection.SetValue(value, absl::Cord());
  EXPECT_EQ(reflection.GetValue(*value, scratch), "");
}

TEST_F(ReflectionTest, StringValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::StringValue>();
  EXPECT_EQ(StringValueReflection::GetValue(*value), "");
  StringValueReflection::SetValue(value, "Hello World!");
  EXPECT_EQ(StringValueReflection::GetValue(*value), "Hello World!");
}

TEST_F(ReflectionTest, StringValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::StringValue>();
  std::string scratch;
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetStringValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetValue(*value, scratch), "");
  reflection.SetValue(value, "Hello World!");
  EXPECT_EQ(reflection.GetValue(*value, scratch), "Hello World!");
  reflection.SetValue(value, absl::Cord());
  EXPECT_EQ(reflection.GetValue(*value, scratch), "");
}

TEST_F(ReflectionTest, Any_Generated) {
  auto* value = MakeGenerated<google::protobuf::Any>();
  EXPECT_EQ(AnyReflection::GetTypeUrl(*value), "");
  AnyReflection::SetTypeUrl(value, "Hello World!");
  EXPECT_EQ(AnyReflection::GetTypeUrl(*value), "Hello World!");
  EXPECT_EQ(AnyReflection::GetValue(*value), "");
  AnyReflection::SetValue(value, absl::Cord("Hello World!"));
  EXPECT_EQ(AnyReflection::GetValue(*value), "Hello World!");
}

TEST_F(ReflectionTest, Any_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Any>();
  std::string scratch;
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetAnyReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetTypeUrl(*value, scratch), "");
  reflection.SetTypeUrl(value, "Hello World!");
  EXPECT_EQ(reflection.GetTypeUrl(*value, scratch), "Hello World!");
  EXPECT_EQ(reflection.GetValue(*value, scratch), "");
  reflection.SetValue(value, absl::Cord("Hello World!"));
  EXPECT_EQ(reflection.GetValue(*value, scratch), "Hello World!");
}

TEST_F(ReflectionTest, Duration_Generated) {
  auto* value = MakeGenerated<google::protobuf::Duration>();
  EXPECT_EQ(DurationReflection::GetSeconds(*value), 0);
  DurationReflection::SetSeconds(value, 1);
  EXPECT_EQ(DurationReflection::GetSeconds(*value), 1);
  EXPECT_EQ(DurationReflection::GetNanos(*value), 0);
  DurationReflection::SetNanos(value, 1);
  EXPECT_EQ(DurationReflection::GetNanos(*value), 1);

  EXPECT_THAT(DurationReflection::SetFromAbslDuration(
                  value, absl::Seconds(1) + absl::Nanoseconds(1)),
              IsOk());
  EXPECT_EQ(value->seconds(), 1);
  EXPECT_EQ(value->nanos(), 1);

  EXPECT_THAT(
      DurationReflection::SetFromAbslDuration(value, absl::InfiniteDuration()),
      StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      DurationReflection::SetFromAbslDuration(value, -absl::InfiniteDuration()),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ReflectionTest, Duration_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Duration>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetDurationReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetSeconds(*value), 0);
  reflection.SetSeconds(value, 1);
  EXPECT_EQ(reflection.GetSeconds(*value), 1);
  EXPECT_EQ(reflection.GetNanos(*value), 0);
  reflection.SetNanos(value, 1);
  EXPECT_EQ(reflection.GetNanos(*value), 1);

  EXPECT_THAT(reflection.SetFromAbslDuration(
                  value, absl::Seconds(1) + absl::Nanoseconds(1)),
              IsOk());
  EXPECT_EQ(reflection.GetSeconds(*value), 1);
  EXPECT_EQ(reflection.GetNanos(*value), 1);

  EXPECT_THAT(reflection.SetFromAbslDuration(value, absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(reflection.SetFromAbslDuration(value, -absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ReflectionTest, Timestamp_Generated) {
  auto* value = MakeGenerated<google::protobuf::Timestamp>();
  EXPECT_EQ(TimestampReflection::GetSeconds(*value), 0);
  TimestampReflection::SetSeconds(value, 1);
  EXPECT_EQ(TimestampReflection::GetSeconds(*value), 1);
  EXPECT_EQ(TimestampReflection::GetNanos(*value), 0);
  TimestampReflection::SetNanos(value, 1);
  EXPECT_EQ(TimestampReflection::GetNanos(*value), 1);

  EXPECT_THAT(
      TimestampReflection::SetFromAbslTime(
          value, absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)),
      IsOk());
  EXPECT_EQ(value->seconds(), 1);
  EXPECT_EQ(value->nanos(), 1);

  EXPECT_THAT(
      TimestampReflection::SetFromAbslTime(value, absl::InfiniteFuture()),
      StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(TimestampReflection::SetFromAbslTime(value, absl::InfinitePast()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ReflectionTest, Timestamp_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Timestamp>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetTimestampReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetSeconds(*value), 0);
  reflection.SetSeconds(value, 1);
  EXPECT_EQ(reflection.GetSeconds(*value), 1);
  EXPECT_EQ(reflection.GetNanos(*value), 0);
  reflection.SetNanos(value, 1);
  EXPECT_EQ(reflection.GetNanos(*value), 1);

  EXPECT_THAT(
      reflection.SetFromAbslTime(
          value, absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)),
      IsOk());
  EXPECT_EQ(reflection.GetSeconds(*value), 1);
  EXPECT_EQ(reflection.GetNanos(*value), 1);

  EXPECT_THAT(reflection.SetFromAbslTime(value, absl::InfiniteFuture()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(reflection.SetFromAbslTime(value, absl::InfinitePast()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ReflectionTest, Value_Generated) {
  auto* value = MakeGenerated<google::protobuf::Value>();
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::KIND_NOT_SET);
  ValueReflection::SetNullValue(value);
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::kNullValue);
  ValueReflection::SetBoolValue(value, true);
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::kBoolValue);
  EXPECT_EQ(ValueReflection::GetBoolValue(*value), true);
  ValueReflection::SetNumberValue(value, 1.0);
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::kNumberValue);
  EXPECT_EQ(ValueReflection::GetNumberValue(*value), 1.0);
  ValueReflection::SetStringValue(value, "Hello World!");
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::kStringValue);
  EXPECT_EQ(ValueReflection::GetStringValue(*value), "Hello World!");
  ValueReflection::MutableListValue(value);
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::kListValue);
  EXPECT_EQ(ValueReflection::GetListValue(*value).ByteSizeLong(), 0);
  ValueReflection::MutableStructValue(value);
  EXPECT_EQ(ValueReflection::GetKindCase(*value),
            google::protobuf::Value::kStructValue);
  EXPECT_EQ(ValueReflection::GetStructValue(*value).ByteSizeLong(), 0);
}

TEST_F(ReflectionTest, Value_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Value>();
  std::string scratch;
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::KIND_NOT_SET);
  reflection.SetNullValue(value);
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::kNullValue);
  reflection.SetBoolValue(value, true);
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::kBoolValue);
  EXPECT_EQ(reflection.GetBoolValue(*value), true);
  reflection.SetNumberValue(value, 1.0);
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::kNumberValue);
  EXPECT_EQ(reflection.GetNumberValue(*value), 1.0);
  reflection.SetStringValue(value, "Hello World!");
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::kStringValue);
  EXPECT_EQ(reflection.GetStringValue(*value, scratch), "Hello World!");
  reflection.MutableListValue(value);
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::kListValue);
  EXPECT_EQ(reflection.GetListValue(*value).ByteSizeLong(), 0);
  EXPECT_THAT(reflection.ReleaseListValue(value), NotNull());
  reflection.MutableStructValue(value);
  EXPECT_EQ(reflection.GetKindCase(*value),
            google::protobuf::Value::kStructValue);
  EXPECT_EQ(reflection.GetStructValue(*value).ByteSizeLong(), 0);
  EXPECT_THAT(reflection.ReleaseStructValue(value), NotNull());
}

TEST_F(ReflectionTest, ListValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::ListValue>();
  EXPECT_EQ(ListValueReflection::ValuesSize(*value), 0);
  EXPECT_EQ(ListValueReflection::Values(*value).size(), 0);
  EXPECT_EQ(ListValueReflection::MutableValues(value).size(), 0);
}

TEST_F(ReflectionTest, ListValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::ListValue>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetListValueReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.ValuesSize(*value), 0);
  EXPECT_EQ(reflection.Values(*value).size(), 0);
  EXPECT_EQ(reflection.MutableValues(value).size(), 0);
}

TEST_F(ReflectionTest, StructValue_Generated) {
  auto* value = MakeGenerated<google::protobuf::Struct>();
  EXPECT_EQ(StructReflection::FieldsSize(*value), 0);
  EXPECT_EQ(StructReflection::BeginFields(*value),
            StructReflection::EndFields(*value));
  EXPECT_FALSE(StructReflection::ContainsField(*value, "foo"));
  EXPECT_THAT(StructReflection::FindField(*value, "foo"), IsNull());
  EXPECT_THAT(StructReflection::InsertField(value, "foo"), NotNull());
  EXPECT_TRUE(StructReflection::DeleteField(value, "foo"));
}

TEST_F(ReflectionTest, StructValue_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::Struct>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetStructReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.FieldsSize(*value), 0);
  EXPECT_EQ(reflection.BeginFields(*value), reflection.EndFields(*value));
  EXPECT_FALSE(reflection.ContainsField(*value, "foo"));
  EXPECT_THAT(reflection.FindField(*value, "foo"), IsNull());
  EXPECT_THAT(reflection.InsertField(value, "foo"), NotNull());
  EXPECT_TRUE(reflection.DeleteField(value, "foo"));
}

TEST_F(ReflectionTest, FieldMask_Generated) {
  auto* value = MakeGenerated<google::protobuf::FieldMask>();
  EXPECT_EQ(FieldMaskReflection::PathsSize(*value), 0);
  value->add_paths("foo");
  EXPECT_EQ(FieldMaskReflection::PathsSize(*value), 1);
  EXPECT_EQ(FieldMaskReflection::Paths(*value, 0), "foo");
}

TEST_F(ReflectionTest, FieldMask_Dynamic) {
  auto* value = MakeDynamic<google::protobuf::FieldMask>();
  ASSERT_OK_AND_ASSIGN(
      auto reflection,
      GetFieldMaskReflection(ABSL_DIE_IF_NULL(value->GetDescriptor())));
  EXPECT_EQ(reflection.PathsSize(*value), 0);
  value->GetReflection()->AddString(
      &*value,
      ABSL_DIE_IF_NULL(value->GetDescriptor()->FindFieldByName("paths")),
      "foo");
  EXPECT_EQ(reflection.PathsSize(*value), 1);
  EXPECT_EQ(reflection.Paths(*value, 0, scratch_space()), "foo");
}

TEST_F(ReflectionTest, NullValue_MissingValue) {
  google::protobuf::DescriptorPool descriptor_pool;
  {
    google::protobuf::FileDescriptorProto file_proto;
    file_proto.set_name("google/protobuf/struct.proto");
    file_proto.set_syntax("editions");
    file_proto.set_edition(google::protobuf::EDITION_2023);
    file_proto.set_package("google.protobuf");
    auto* enum_proto = file_proto.add_enum_type();
    enum_proto->set_name("NullValue");
    auto* value_proto = enum_proto->add_value();
    value_proto->set_number(1);
    value_proto->set_name("NULL_VALUE");
    enum_proto->mutable_options()->mutable_features()->set_enum_type(
        google::protobuf::FeatureSet::CLOSED);
    ASSERT_THAT(descriptor_pool.BuildFile(file_proto), NotNull());
  }
  EXPECT_THAT(
      NullValueReflection().Initialize(&descriptor_pool),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("well known protocol buffer enum missing value: ")));
}

TEST_F(ReflectionTest, NullValue_MultipleValues) {
  google::protobuf::DescriptorPool descriptor_pool;
  {
    google::protobuf::FileDescriptorProto file_proto;
    file_proto.set_name("google/protobuf/struct.proto");
    file_proto.set_syntax("proto3");
    file_proto.set_package("google.protobuf");
    auto* enum_proto = file_proto.add_enum_type();
    enum_proto->set_name("NullValue");
    auto* value_proto = enum_proto->add_value();
    value_proto->set_number(0);
    value_proto->set_name("NULL_VALUE");
    value_proto = enum_proto->add_value();
    value_proto->set_number(1);
    value_proto->set_name("NULL_VALUE2");
    ASSERT_THAT(descriptor_pool.BuildFile(file_proto), NotNull());
  }
  EXPECT_THAT(
      NullValueReflection().Initialize(&descriptor_pool),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("well known protocol buffer enum has multiple values: ")));
}

TEST_F(ReflectionTest, EnumDescriptorMissing) {
  google::protobuf::DescriptorPool descriptor_pool;
  EXPECT_THAT(NullValueReflection().Initialize(&descriptor_pool),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("descriptor missing for protocol buffer enum "
                                 "well known type: ")));
}

TEST_F(ReflectionTest, MessageDescriptorMissing) {
  google::protobuf::DescriptorPool descriptor_pool;
  EXPECT_THAT(BoolValueReflection().Initialize(&descriptor_pool),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("descriptor missing for protocol buffer "
                                 "message well known type: ")));
}

class AdaptFromMessageTest : public Test {
 public:
  google::protobuf::Arena* absl_nonnull arena() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return &arena_;
  }

  std::string& scratch_space() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return scratch_space_;
  }

  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  google::protobuf::MessageFactory* absl_nonnull message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  google::protobuf::Message* absl_nonnull MakeDynamic() {
    const auto* descriptor_pool = GetTestingDescriptorPool();
    const auto* descriptor =
        ABSL_DIE_IF_NULL(descriptor_pool->FindMessageTypeByName(
            internal::MessageTypeNameFor<T>()));
    const auto* prototype =
        ABSL_DIE_IF_NULL(GetTestingMessageFactory()->GetPrototype(descriptor));
    return prototype->New(arena());
  }

  template <typename T>
  google::protobuf::Message* DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  absl::StatusOr<Value> AdaptFromMessage(const google::protobuf::Message& message) {
    return well_known_types::AdaptFromMessage(
        arena(), message, descriptor_pool(), message_factory(),
        scratch_space());
  }

 private:
  google::protobuf::Arena arena_;
  std::string scratch_space_;
};

TEST_F(AdaptFromMessageTest, BoolValue) {
  auto message =
      DynamicParseTextProto<google::protobuf::BoolValue>(R"pb(value: true)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<bool>(true)));
}

TEST_F(AdaptFromMessageTest, Int32Value) {
  auto message =
      DynamicParseTextProto<google::protobuf::Int32Value>(R"pb(value: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<int32_t>(1)));
}

TEST_F(AdaptFromMessageTest, Int64Value) {
  auto message =
      DynamicParseTextProto<google::protobuf::Int64Value>(R"pb(value: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<int64_t>(1)));
}

TEST_F(AdaptFromMessageTest, UInt32Value) {
  auto message =
      DynamicParseTextProto<google::protobuf::UInt32Value>(R"pb(value: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<uint32_t>(1)));
}

TEST_F(AdaptFromMessageTest, UInt64Value) {
  auto message =
      DynamicParseTextProto<google::protobuf::UInt64Value>(R"pb(value: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<uint64_t>(1)));
}

TEST_F(AdaptFromMessageTest, FloatValue) {
  auto message =
      DynamicParseTextProto<google::protobuf::FloatValue>(R"pb(value: 1.0)pb");
  EXPECT_THAT(AdaptFromMessage(*message), IsOkAndHolds(VariantWith<float>(1)));
}

TEST_F(AdaptFromMessageTest, DoubleValue) {
  auto message =
      DynamicParseTextProto<google::protobuf::DoubleValue>(R"pb(value: 1.0)pb");
  EXPECT_THAT(AdaptFromMessage(*message), IsOkAndHolds(VariantWith<double>(1)));
}

TEST_F(AdaptFromMessageTest, BytesValue) {
  auto message = DynamicParseTextProto<google::protobuf::BytesValue>(
      R"pb(value: "foo")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<BytesValue>(BytesValue("foo"))));
}

TEST_F(AdaptFromMessageTest, StringValue) {
  auto message = DynamicParseTextProto<google::protobuf::StringValue>(
      R"pb(value: "foo")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<StringValue>(StringValue("foo"))));
}

TEST_F(AdaptFromMessageTest, Duration) {
  auto message = DynamicParseTextProto<google::protobuf::Duration>(
      R"pb(seconds: 1 nanos: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<absl::Duration>(absl::Seconds(1) +
                                                       absl::Nanoseconds(1))));
}

TEST_F(AdaptFromMessageTest, Duration_SecondsOutOfRange) {
  auto message = DynamicParseTextProto<google::protobuf::Duration>(
      R"pb(seconds: 0x7fffffffffffffff nanos: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid duration seconds: ")));
}

TEST_F(AdaptFromMessageTest, Duration_NanosOutOfRange) {
  auto message = DynamicParseTextProto<google::protobuf::Duration>(
      R"pb(seconds: 1 nanos: 0x7fffffff)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid duration nanoseconds: ")));
}

TEST_F(AdaptFromMessageTest, Duration_SignMismatch) {
  auto message =
      DynamicParseTextProto<google::protobuf::Duration>(R"pb(seconds: -1
                                                             nanos: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("duration sign mismatch: ")));
}

TEST_F(AdaptFromMessageTest, Timestamp) {
  auto message =
      DynamicParseTextProto<google::protobuf::Timestamp>(R"pb(seconds: 1
                                                              nanos: 1)pb");
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<absl::Time>(
          absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1))));
}

TEST_F(AdaptFromMessageTest, Timestamp_SecondsOutOfRange) {
  auto message = DynamicParseTextProto<google::protobuf::Timestamp>(
      R"pb(seconds: 0x7fffffffffffffff nanos: 1)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid timestamp seconds: ")));
}

TEST_F(AdaptFromMessageTest, Timestamp_NanosOutOfRange) {
  auto message = DynamicParseTextProto<google::protobuf::Timestamp>(
      R"pb(seconds: 1 nanos: 0x7fffffff)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid timestamp nanoseconds: ")));
}

TEST_F(AdaptFromMessageTest, Value_NullValue) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(null_value: NULL_VALUE)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<std::nullptr_t>(nullptr)));
}

TEST_F(AdaptFromMessageTest, Value_BoolValue) {
  auto message =
      DynamicParseTextProto<google::protobuf::Value>(R"pb(bool_value: true)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<bool>(true)));
}

TEST_F(AdaptFromMessageTest, Value_NumberValue) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(number_value: 1.0)pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<double>(1.0)));
}

TEST_F(AdaptFromMessageTest, Value_StringValue) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(string_value: "foo")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<StringValue>(StringValue("foo"))));
}

TEST_F(AdaptFromMessageTest, Value_ListValue) {
  auto message =
      DynamicParseTextProto<google::protobuf::Value>(R"pb(list_value: {})pb");
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<ListValue>(VariantWith<ListValueConstRef>(_))));
}

TEST_F(AdaptFromMessageTest, Value_StructValue) {
  auto message =
      DynamicParseTextProto<google::protobuf::Value>(R"pb(struct_value: {})pb");
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<Struct>(VariantWith<StructConstRef>(_))));
}

TEST_F(AdaptFromMessageTest, ListValue) {
  auto message = DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb");
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<ListValue>(VariantWith<ListValueConstRef>(_))));
}

TEST_F(AdaptFromMessageTest, Struct) {
  auto message = DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb");
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<Struct>(VariantWith<StructConstRef>(_))));
}

TEST_F(AdaptFromMessageTest, TestAllTypesProto3) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<absl::monostate>(absl::monostate())));
}

TEST_F(AdaptFromMessageTest, Any_BoolValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.BoolValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<bool>(false)));
}

TEST_F(AdaptFromMessageTest, Any_Int32Value) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Int32Value")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<int32_t>(0)));
}

TEST_F(AdaptFromMessageTest, Any_Int64Value) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Int64Value")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<int64_t>(0)));
}

TEST_F(AdaptFromMessageTest, Any_UInt32Value) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.UInt32Value")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<uint32_t>(0)));
}

TEST_F(AdaptFromMessageTest, Any_UInt64Value) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.UInt64Value")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<uint64_t>(0)));
}

TEST_F(AdaptFromMessageTest, Any_FloatValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.FloatValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message), IsOkAndHolds(VariantWith<float>(0)));
}

TEST_F(AdaptFromMessageTest, Any_DoubleValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.DoubleValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message), IsOkAndHolds(VariantWith<double>(0)));
}

TEST_F(AdaptFromMessageTest, Any_BytesValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.BytesValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<BytesValue>(BytesValue())));
}

TEST_F(AdaptFromMessageTest, Any_StringValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.StringValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<StringValue>(StringValue())));
}

TEST_F(AdaptFromMessageTest, Any_Duration) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Duration")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<absl::Duration>(absl::ZeroDuration())));
}

TEST_F(AdaptFromMessageTest, Any_Timestamp) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Timestamp")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<absl::Time>(absl::UnixEpoch())));
}

TEST_F(AdaptFromMessageTest, Any_Value_NullValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Value")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<std::nullptr_t>(nullptr)));
}

TEST_F(AdaptFromMessageTest, Any_Value_BoolValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(

      R"pb(type_url: "type.googleapis.com/google.protobuf.Value"
           value: "\x20\x01")pb");  // bool_value: true
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<bool>(true)));
}

TEST_F(AdaptFromMessageTest, Any_Value_NumberValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Value"
           value: "\x11\x00\x00\x00\x00\x00\x00\x00\x00")pb");  // number_value:
                                                                 // 1.0
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<double>(0.0)));
}

TEST_F(AdaptFromMessageTest, Any_Value_StringValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Value"
           value: "\x1a\x03\x66\x6f\x6f")pb");  // string_value: "foo"
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<StringValue>(StringValue("foo"))));
}

TEST_F(AdaptFromMessageTest, Any_Value_ListValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Value"
           value: "\x32\x00")pb");  // list_value: {}
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<ListValue>(
                  VariantWith<ListValuePtr>(NotNull()))));
}

TEST_F(AdaptFromMessageTest, Any_Value_StructValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Value"
           value: "\x2a\x00")pb");  // struct_value: {}
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<Struct>(VariantWith<StructPtr>(NotNull()))));
}

TEST_F(AdaptFromMessageTest, Any_ListValue) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.ListValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<ListValue>(
                  VariantWith<ListValuePtr>(NotNull()))));
}

TEST_F(AdaptFromMessageTest, Any_Struct) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/google.protobuf.Struct")pb");
  EXPECT_THAT(
      AdaptFromMessage(*message),
      IsOkAndHolds(VariantWith<Struct>(VariantWith<StructPtr>(NotNull()))));
}

TEST_F(AdaptFromMessageTest, Any_TestAllTypesProto3) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              IsOkAndHolds(VariantWith<Unique<google::protobuf::Message>>(NotNull())));
}

TEST_F(AdaptFromMessageTest, Any_BadTypeUrlDomain) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.example.com/google.protobuf.BoolValue")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("unable to find descriptor for type URL: ")));
}

TEST_F(AdaptFromMessageTest, Any_UnknownMessage) {
  auto message = DynamicParseTextProto<google::protobuf::Any>(
      R"pb(type_url: "type.googleapis.com/message.that.does.not.Exist")pb");
  EXPECT_THAT(AdaptFromMessage(*message),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("unable to find descriptor for type name: ")));
}

}  // namespace
}  // namespace cel::well_known_types
