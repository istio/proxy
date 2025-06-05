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

#include "internal/json.h"

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "internal/equals_text_proto.h"
#include "internal/message_type_name.h"
#include "internal/parse_text_proto.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::internal::test::EqualsProto;
using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::Test;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class CheckJsonTest : public Test {
 public:
  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  T* MakeGenerated() {
    return google::protobuf::Arena::Create<T>(arena());
  }

  template <typename T>
  google::protobuf::Message* MakeDynamic() {
    const auto* descriptor = ABSL_DIE_IF_NULL(
        descriptor_pool()->FindMessageTypeByName(MessageTypeNameFor<T>()));
    const auto* prototype =
        ABSL_DIE_IF_NULL(message_factory()->GetPrototype(descriptor));
    return ABSL_DIE_IF_NULL(prototype->New(arena()));
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(CheckJsonTest, Value_Generated) {
  EXPECT_THAT(CheckJson(*MakeGenerated<google::protobuf::Value>()), IsOk());
}

TEST_F(CheckJsonTest, Value_Dynamic) {
  EXPECT_THAT(CheckJson(*MakeDynamic<google::protobuf::Value>()), IsOk());
}

TEST_F(CheckJsonTest, ListValue_Generated) {
  EXPECT_THAT(CheckJsonList(*MakeGenerated<google::protobuf::ListValue>()),
              IsOk());
}

TEST_F(CheckJsonTest, ListValue_Dynamic) {
  EXPECT_THAT(CheckJsonList(*MakeDynamic<google::protobuf::ListValue>()),
              IsOk());
}

TEST_F(CheckJsonTest, Struct_Generated) {
  EXPECT_THAT(CheckJsonMap(*MakeGenerated<google::protobuf::Struct>()), IsOk());
}

TEST_F(CheckJsonTest, Struct_Dynamic) {
  EXPECT_THAT(CheckJsonMap(*MakeDynamic<google::protobuf::Struct>()), IsOk());
}

class MessageToJsonTest : public Test {
 public:
  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  T* MakeGenerated() {
    return google::protobuf::Arena::Create<T>(arena());
  }

  template <typename T>
  google::protobuf::Message* MakeDynamic() {
    const auto* descriptor = ABSL_DIE_IF_NULL(
        descriptor_pool()->FindMessageTypeByName(MessageTypeNameFor<T>()));
    const auto* prototype =
        ABSL_DIE_IF_NULL(message_factory()->GetPrototype(descriptor));
    return ABSL_DIE_IF_NULL(prototype->New(arena()));
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto EqualsTextProto(absl::string_view text) {
    return ::cel::internal::EqualsTextProto<T>(arena(), text, descriptor_pool(),
                                               message_factory());
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(MessageToJsonTest, BoolValue_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::BoolValue>(
                                R"pb(value: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(bool_value: true)pb"));
}

TEST_F(MessageToJsonTest, BoolValue_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::BoolValue>(
                                R"pb(value: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(bool_value: true)pb"));
}

TEST_F(MessageToJsonTest, Int32Value_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::Int32Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, Int32Value_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::Int32Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, Int64Value_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::Int64Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, Int64Value_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::Int64Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, UInt32Value_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::UInt32Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, UInt32Value_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::UInt32Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, UInt64Value_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::UInt64Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, UInt64Value_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::UInt64Value>(
                        R"pb(value: 1)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, FloatValue_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::FloatValue>(
                        R"pb(value: 1.0)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, FloatValue_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::FloatValue>(
                        R"pb(value: 1.0)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, DoubleValue_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::DoubleValue>(
                        R"pb(value: 1.0)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, DoubleValue_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::DoubleValue>(
                        R"pb(value: 1.0)pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(number_value: 1.0)pb"));
}

TEST_F(MessageToJsonTest, BytesValue_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::BytesValue>(
                        R"pb(value: "foo")pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "Zm9v")pb"));
}

TEST_F(MessageToJsonTest, BytesValue_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::BytesValue>(
                        R"pb(value: "foo")pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "Zm9v")pb"));
}

TEST_F(MessageToJsonTest, StringValue_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::StringValue>(
                        R"pb(value: "foo")pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "foo")pb"));
}

TEST_F(MessageToJsonTest, StringValue_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::StringValue>(
                        R"pb(value: "foo")pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "foo")pb"));
}

TEST_F(MessageToJsonTest, Duration_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Duration>(
                                R"pb(seconds: 1 nanos: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "1.000000001s")pb"));
}

TEST_F(MessageToJsonTest, Duration_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Duration>(
                                R"pb(seconds: 1 nanos: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "1.000000001s")pb"));
}

TEST_F(MessageToJsonTest, Timestamp_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Timestamp>(
                                R"pb(seconds: 1 nanos: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result,
              EqualsTextProto<google::protobuf::Value>(
                  R"pb(string_value: "1970-01-01T00:00:01.000000001Z")pb"));
}

TEST_F(MessageToJsonTest, Timestamp_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Timestamp>(
                                R"pb(seconds: 1 nanos: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result,
              EqualsTextProto<google::protobuf::Value>(
                  R"pb(string_value: "1970-01-01T00:00:01.000000001Z")pb"));
}

TEST_F(MessageToJsonTest, Value_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Value>(
                                R"pb(bool_value: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(bool_value: true)pb"));
}

TEST_F(MessageToJsonTest, Value_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Value>(
                                R"pb(bool_value: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(bool_value: true)pb"));
}

TEST_F(MessageToJsonTest, ListValue_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::ListValue>(
                                R"pb(values { bool_value: true })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result,
              EqualsTextProto<google::protobuf::Value>(
                  R"pb(list_value: { values { bool_value: true } })pb"));
}

TEST_F(MessageToJsonTest, ListValue_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::ListValue>(
                                R"pb(values { bool_value: true })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result,
              EqualsTextProto<google::protobuf::Value>(
                  R"pb(list_value: { values { bool_value: true } })pb"));
}

TEST_F(MessageToJsonTest, Struct_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Struct>(
                                R"pb(fields {
                                       key: "foo"
                                       value: { bool_value: true }
                                     })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "foo"
                                    value: { bool_value: true }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, Struct_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::Struct>(
                                R"pb(fields {
                                       key: "foo"
                                       value: { bool_value: true }
                                     })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "foo"
                                    value: { bool_value: true }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, FieldMask_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::FieldMask>(
                                R"pb(paths: "foo" paths: "bar")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "foo,bar")pb"));
}

TEST_F(MessageToJsonTest, FieldMask_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::FieldMask>(
                                R"pb(paths: "foo" paths: "bar")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(string_value: "foo,bar")pb"));
}

TEST_F(MessageToJsonTest, FieldMask_BadUpperCase) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<google::protobuf::FieldMask>(
                        R"pb(paths: "Foo")pb"),
                    descriptor_pool(), message_factory(), result),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("field mask path name contains uppercase letters")));
}

TEST_F(MessageToJsonTest, FieldMask_BadUnderscoreUpperCase) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::FieldMask>(
                                R"pb(paths: "foo_?")pb"),
                            descriptor_pool(), message_factory(), result),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("field mask path contains '_' not followed by "
                                 "a lowercase letter")));
}

TEST_F(MessageToJsonTest, FieldMask_BadTrailingUnderscore) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<google::protobuf::FieldMask>(
                                R"pb(paths: "foo_")pb"),
                            descriptor_pool(), message_factory(), result),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("field mask path contains trailing '_'")));
}

TEST_F(MessageToJsonTest, Any_WellKnownType_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<google::protobuf::Any>(
              R"pb(type_url: "type.googleapis.com/google.protobuf.BoolValue"
                   value: "\x08\x01")pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "@type"
                   value: {
                     string_value: "type.googleapis.com/google.protobuf.BoolValue"
                   }
                 }
                 fields {
                   key: "value"
                   value: { bool_value: true }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, Any_WellKnownType_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<google::protobuf::Any>(
              R"pb(type_url: "type.googleapis.com/google.protobuf.BoolValue"
                   value: "\x08\x01")pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "@type"
                   value: {
                     string_value: "type.googleapis.com/google.protobuf.BoolValue"
                   }
                 }
                 fields {
                   key: "value"
                   value: { bool_value: true }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, Any_Empty_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<google::protobuf::Any>(
              R"pb(type_url: "type.googleapis.com/google.protobuf.Empty")pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "@type"
                   value: {
                     string_value: "type.googleapis.com/google.protobuf.Empty"
                   }
                 }
                 fields {
                   key: "value"
                   value: { struct_value: {} }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, Any_Empty_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<google::protobuf::Any>(
              R"pb(type_url: "type.googleapis.com/google.protobuf.Empty")pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "@type"
                   value: {
                     string_value: "type.googleapis.com/google.protobuf.Empty"
                   }
                 }
                 fields {
                   key: "value"
                   value: { struct_value: {} }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, Any_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<google::protobuf::Any>(
              R"pb(type_url: "type.googleapis.com/google.api.expr.test.v1.proto3.TestAllTypes"
                   value: "\x68\x01")pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "@type"
                   value: {
                     string_value: "type.googleapis.com/google.api.expr.test.v1.proto3.TestAllTypes"
                   }
                 }
                 fields {
                   key: "singleBool"
                   value: { bool_value: true }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, Any_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<google::protobuf::Any>(
              R"pb(type_url: "type.googleapis.com/google.api.expr.test.v1.proto3.TestAllTypes"
                   value: "\x68\x01")pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "@type"
                   value: {
                     string_value: "type.googleapis.com/google.api.expr.test.v1.proto3.TestAllTypes"
                   }
                 }
                 fields {
                   key: "singleBool"
                   value: { bool_value: true }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Bool_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_bool: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleBool"
                                    value: { bool_value: true }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Bool_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_bool: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleBool"
                                    value: { bool_value: true }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Int32_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_int32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleInt32"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Int32_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_int32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleInt32"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Int64_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_int64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleInt64"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Int64_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_int64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleInt64"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_UInt32_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_uint32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleUint32"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_UInt32_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_uint32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleUint32"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_UInt64_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_uint64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleUint64"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_UInt64_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_uint64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleUint64"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Float_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_float: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleFloat"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Float_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_float: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleFloat"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Double_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_double: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleDouble"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Double_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_double: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleDouble"
                                    value: { number_value: 1.0 }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Bytes_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_bytes: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleBytes"
                                    value: { string_value: "Zm9v" }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Bytes_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_bytes: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleBytes"
                                    value: { string_value: "Zm9v" }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_String_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_string: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleString"
                                    value: { string_value: "foo" }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_String_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(single_string: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "singleString"
                                    value: { string_value: "foo" }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Message_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(standalone_message: { bb: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "standaloneMessage"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "bb"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Message_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(standalone_message: { bb: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "standaloneMessage"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "bb"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Enum_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(standalone_enum: BAR)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "standaloneEnum"
                                    value: { string_value: "BAR" }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_Enum_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(standalone_enum: BAR)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "standaloneEnum"
                                    value: { string_value: "BAR" }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedBool_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_bool: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedBool"
                   value: { list_value: { values: { bool_value: true } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedBool_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_bool: true)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedBool"
                   value: { list_value: { values: { bool_value: true } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedInt32_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_int32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedInt32"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedInt32_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_int32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedInt32"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedInt64_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_int64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedInt64"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedInt64_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_int64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedInt64"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedUInt32_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_uint32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedUint32"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedUInt32_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_uint32: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedUint32"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedUInt64_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_uint64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedUint64"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedUInt64_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_uint64: 1)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedUint64"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedFloat_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_float: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedFloat"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedFloat_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_float: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedFloat"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedDouble_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_double: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedDouble"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedDouble_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_double: 1.0)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedDouble"
                   value: { list_value: { values: { number_value: 1.0 } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedBytes_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_bytes: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedBytes"
                   value: { list_value: { values: { string_value: "Zm9v" } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedBytes_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_bytes: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedBytes"
                   value: { list_value: { values: { string_value: "Zm9v" } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedString_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_string: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedString"
                   value: { list_value: { values: { string_value: "foo" } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedString_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_string: "foo")pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedString"
                   value: { list_value: { values: { string_value: "foo" } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedMessage_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_nested_message: { bb: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "repeatedNestedMessage"
                                    value: {
                                      list_value: {
                                        values: {
                                          struct_value: {
                                            fields {
                                              key: "bb"
                                              value: { number_value: 1.0 }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedMessage_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_nested_message: { bb: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "repeatedNestedMessage"
                                    value: {
                                      list_value: {
                                        values: {
                                          struct_value: {
                                            fields {
                                              key: "bb"
                                              value: { number_value: 1.0 }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedEnum_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_nested_enum: BAR)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedNestedEnum"
                   value: { list_value: { values: { string_value: "BAR" } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedEnum_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_nested_enum: BAR)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedNestedEnum"
                   value: { list_value: { values: { string_value: "BAR" } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedNull_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_null_value: NULL_VALUE)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedNullValue"
                   value: { list_value: { values: { null_value: NULL_VALUE } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_RepeatedNull_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(repeated_null_value: NULL_VALUE)pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(
      *result,
      EqualsTextProto<google::protobuf::Value>(
          R"pb(struct_value: {
                 fields {
                   key: "repeatedNullValue"
                   value: { list_value: { values: { null_value: NULL_VALUE } } }
                 }
               })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapBoolBool_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_bool_bool: { key: true value: true })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapBoolBool"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "true"
                                          value: { bool_value: true }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapBoolBool_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_bool_bool: { key: true value: true })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapBoolBool"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "true"
                                          value: { bool_value: true }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapInt32Int32_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(map_int32_int32: { key: 1 value: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapInt32Int32"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapInt32Int32_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(map_int32_int32: { key: 1 value: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapInt32Int32"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapInt64Int64_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(map_int64_int64: { key: 1 value: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapInt64Int64"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapInt64Int64_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(map_int64_int64: { key: 1 value: 1 })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapInt64Int64"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapUInt32UInt32_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_uint32_uint32: { key: 1 value: 1 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapUint32Uint32"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapUInt32UInt32_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_uint32_uint32: { key: 1 value: 1 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapUint32Uint32"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapUInt64UInt64_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_uint64_uint64: { key: 1 value: 1 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapUint64Uint64"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapUInt64UInt64_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_uint64_uint64: { key: 1 value: 1 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapUint64Uint64"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "1"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringString_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(
                  *DynamicParseTextProto<TestAllTypesProto3>(
                      R"pb(map_string_string: { key: "foo" value: "bar" })pb"),
                  descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringString"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { string_value: "bar" }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringString_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(
                  *DynamicParseTextProto<TestAllTypesProto3>(
                      R"pb(map_string_string: { key: "foo" value: "bar" })pb"),
                  descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringString"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { string_value: "bar" }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringFloat_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_float: { key: "foo" value: 1.0 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringFloat"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringFloat_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_float: { key: "foo" value: 1.0 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringFloat"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringDouble_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_double: { key: "foo" value: 1.0 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringDouble"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringDouble_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_double: { key: "foo" value: 1.0 })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringDouble"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { number_value: 1.0 }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringBytes_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_bytes: { key: "foo" value: "bar" })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringBytes"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { string_value: "YmFy" }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringBytes_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_bytes: { key: "foo" value: "bar" })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringBytes"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { string_value: "YmFy" }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringMessage_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(map_string_message: {
                                       key: "foo"
                                       value: { bb: 1 }
                                     })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringMessage"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {
                                            struct_value: {
                                              fields {
                                                key: "bb"
                                                value: { number_value: 1.0 }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringMessage_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                                R"pb(map_string_message: {
                                       key: "foo"
                                       value: { bb: 1 }
                                     })pb"),
                            descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringMessage"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {
                                            struct_value: {
                                              fields {
                                                key: "bb"
                                                value: { number_value: 1.0 }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringEnum_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_enum: { key: "foo" value: BAR })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringEnum"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { string_value: "BAR" }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringEnum_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(*DynamicParseTextProto<TestAllTypesProto3>(
                        R"pb(map_string_enum: { key: "foo" value: BAR })pb"),
                    descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringEnum"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { string_value: "BAR" }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringNull_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<TestAllTypesProto3>(
              R"pb(map_string_null_value: { key: "foo" value: NULL_VALUE })pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringNullValue"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { null_value: NULL_VALUE }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

TEST_F(MessageToJsonTest, TestAllTypesProto3_MapStringNull_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(
      MessageToJson(
          *DynamicParseTextProto<TestAllTypesProto3>(
              R"pb(map_string_null_value: { key: "foo" value: NULL_VALUE })pb"),
          descriptor_pool(), message_factory(), result),
      IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(struct_value: {
                                  fields {
                                    key: "mapStringNullValue"
                                    value: {
                                      struct_value: {
                                        fields {
                                          key: "foo"
                                          value: { null_value: NULL_VALUE }
                                        }
                                      }
                                    }
                                  }
                                })pb"));
}

class MessageFieldToJsonTest : public Test {
 public:
  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  T* MakeGenerated() {
    return google::protobuf::Arena::Create<T>(arena());
  }

  template <typename T>
  google::protobuf::Message* MakeDynamic() {
    const auto* descriptor = ABSL_DIE_IF_NULL(
        descriptor_pool()->FindMessageTypeByName(MessageTypeNameFor<T>()));
    const auto* prototype =
        ABSL_DIE_IF_NULL(message_factory()->GetPrototype(descriptor));
    return ABSL_DIE_IF_NULL(prototype->New(arena()));
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto EqualsTextProto(absl::string_view text) {
    return ::cel::internal::EqualsTextProto<T>(arena(), text, descriptor_pool(),
                                               message_factory());
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(MessageFieldToJsonTest, TestAllTypesProto3_Generated) {
  auto* result = MakeGenerated<google::protobuf::Value>();
  EXPECT_THAT(MessageFieldToJson(
                  *DynamicParseTextProto<TestAllTypesProto3>(
                      R"pb(single_bool: true)pb"),
                  ABSL_DIE_IF_NULL(
                      ABSL_DIE_IF_NULL(
                          descriptor_pool()->FindMessageTypeByName(
                              "google.api.expr.test.v1.proto3.TestAllTypes"))
                          ->FindFieldByName("single_bool")),
                  descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(bool_value: true)pb"));
}

TEST_F(MessageFieldToJsonTest, TestAllTypesProto3_Dynamic) {
  auto* result = MakeDynamic<google::protobuf::Value>();
  EXPECT_THAT(MessageFieldToJson(
                  *DynamicParseTextProto<TestAllTypesProto3>(
                      R"pb(single_bool: true)pb"),
                  ABSL_DIE_IF_NULL(
                      ABSL_DIE_IF_NULL(
                          descriptor_pool()->FindMessageTypeByName(
                              "google.api.expr.test.v1.proto3.TestAllTypes"))
                          ->FindFieldByName("single_bool")),
                  descriptor_pool(), message_factory(), result),
              IsOk());
  EXPECT_THAT(*result, EqualsTextProto<google::protobuf::Value>(
                           R"pb(bool_value: true)pb"));
}

class JsonDebugStringTest : public Test {
 public:
  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  auto GeneratedParseTextProto(absl::string_view text) {
    return ::cel::internal::GeneratedParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(JsonDebugStringTest, Null_Generated) {
  EXPECT_EQ(JsonDebugString(
                *GeneratedParseTextProto<google::protobuf::Value>(R"pb()pb")),
            "null");
}

TEST_F(JsonDebugStringTest, Null_Dynamic) {
  EXPECT_EQ(JsonDebugString(
                *DynamicParseTextProto<google::protobuf::Value>(R"pb()pb")),
            "null");
}

TEST_F(JsonDebugStringTest, Bool_Generated) {
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(bool_value: false)pb")),
            "false");
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(bool_value: true)pb")),
            "true");
}

TEST_F(JsonDebugStringTest, Bool_Dynamic) {
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(bool_value: false)pb")),
            "false");
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(bool_value: true)pb")),
            "true");
}

TEST_F(JsonDebugStringTest, Number_Generated) {
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(number_value: 1.0)pb")),
            "1.0");
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(number_value: 1.1)pb")),
            "1.1");
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(number_value: infinity)pb")),
            "+infinity");
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(number_value: -infinity)pb")),
            "-infinity");
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(number_value: nan)pb")),
            "nan");
}

TEST_F(JsonDebugStringTest, Number_Dynamic) {
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(number_value: 1.0)pb")),
            "1.0");
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(number_value: 1.1)pb")),
            "1.1");
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(number_value: infinity)pb")),
            "+infinity");
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(number_value: -infinity)pb")),
            "-infinity");
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(number_value: nan)pb")),
            "nan");
}

TEST_F(JsonDebugStringTest, String_Generated) {
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(string_value: "foo")pb")),
            "\"foo\"");
}

TEST_F(JsonDebugStringTest, String_Dynamic) {
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(string_value: "foo")pb")),
            "\"foo\"");
}

TEST_F(JsonDebugStringTest, List_Generated) {
  EXPECT_EQ(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                R"pb(list_value: {
                       values {}
                       values { bool_value: true }
                     })pb")),
            "[null, true]");
  EXPECT_EQ(
      JsonListDebugString(*GeneratedParseTextProto<google::protobuf::ListValue>(
          R"pb(
            values {}
            values { bool_value: true })pb")),
      "[null, true]");
}

TEST_F(JsonDebugStringTest, List_Dynamic) {
  EXPECT_EQ(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                R"pb(list_value: {
                       values {}
                       values { bool_value: true }
                     })pb")),
            "[null, true]");
  EXPECT_EQ(
      JsonListDebugString(*DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(
            values {}
            values { bool_value: true })pb")),
      "[null, true]");
}

TEST_F(JsonDebugStringTest, Struct_Generated) {
  EXPECT_THAT(JsonDebugString(*GeneratedParseTextProto<google::protobuf::Value>(
                  R"pb(struct_value: {
                         fields {
                           key: "foo"
                           value: {}
                         }
                         fields {
                           key: "bar"
                           value: { bool_value: true }
                         }
                       })pb")),
              AnyOf("{\"foo\": null, \"bar\": true}",
                    "{\"bar\": true, \"foo\": null}"));
  EXPECT_THAT(
      JsonMapDebugString(*GeneratedParseTextProto<google::protobuf::Struct>(
          R"pb(
            fields {
              key: "foo"
              value: {}
            }
            fields {
              key: "bar"
              value: { bool_value: true }
            })pb")),
      AnyOf("{\"foo\": null, \"bar\": true}",
            "{\"bar\": true, \"foo\": null}"));
}

TEST_F(JsonDebugStringTest, Struct_Dynamic) {
  EXPECT_THAT(JsonDebugString(*DynamicParseTextProto<google::protobuf::Value>(
                  R"pb(struct_value: {
                         fields {
                           key: "foo"
                           value: {}
                         }
                         fields {
                           key: "bar"
                           value: { bool_value: true }
                         }
                       })pb")),
              AnyOf("{\"foo\": null, \"bar\": true}",
                    "{\"bar\": true, \"foo\": null}"));
  EXPECT_THAT(
      JsonMapDebugString(*DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(
            fields {
              key: "foo"
              value: {}
            }
            fields {
              key: "bar"
              value: { bool_value: true }
            })pb")),
      AnyOf("{\"foo\": null, \"bar\": true}",
            "{\"bar\": true, \"foo\": null}"));
}

class JsonEqualsTest : public Test {
 public:
  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  auto GeneratedParseTextProto(absl::string_view text) {
    return ::cel::internal::GeneratedParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(JsonEqualsTest, Null_Null_Generated_Generated) {
  EXPECT_TRUE(
      JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(R"pb()pb"),
                 *GeneratedParseTextProto<google::protobuf::Value>(R"pb()pb")));
}

TEST_F(JsonEqualsTest, Null_Null_Generated_Dynamic) {
  EXPECT_TRUE(
      JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(R"pb()pb"),
                 *DynamicParseTextProto<google::protobuf::Value>(R"pb()pb")));
}

TEST_F(JsonEqualsTest, Null_Null_Dynamic_Generated) {
  EXPECT_TRUE(
      JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(R"pb()pb"),
                 *GeneratedParseTextProto<google::protobuf::Value>(R"pb()pb")));
}

TEST_F(JsonEqualsTest, Null_Null_Dynamic_Dynamic) {
  EXPECT_TRUE(
      JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(R"pb()pb"),
                 *DynamicParseTextProto<google::protobuf::Value>(R"pb()pb")));
}

TEST_F(JsonEqualsTest, Bool_Bool_Generated_Generated) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb")));
}

TEST_F(JsonEqualsTest, Bool_Bool_Generated_Dynamic) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb")));
}

TEST_F(JsonEqualsTest, Bool_Bool_Dynamic_Generated) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb")));
}

TEST_F(JsonEqualsTest, Bool_Bool_Dynamic_Dynamic) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(bool_value: true)pb")));
}

TEST_F(JsonEqualsTest, Number_Number_Generated_Generated) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb")));
}

TEST_F(JsonEqualsTest, Number_Number_Generated_Dynamic) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb")));
}

TEST_F(JsonEqualsTest, Number_Number_Dynamic_Generated) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb")));
}

TEST_F(JsonEqualsTest, Number_Number_Dynamic_Dynamic) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(number_value: 1.0)pb")));
}

TEST_F(JsonEqualsTest, String_String_Generated_Generated) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb")));
}

TEST_F(JsonEqualsTest, String_String_Generated_Dynamic) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb")));
}

TEST_F(JsonEqualsTest, String_String_Dynamic_Generated) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb")));
}

TEST_F(JsonEqualsTest, String_String_Dynamic_Dynamic) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(string_value: "foo")pb")));
}

TEST_F(JsonEqualsTest, List_List_Generated_Generated) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb"))));
  EXPECT_TRUE(
      JsonListEquals(*GeneratedParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb"),
                     *GeneratedParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb")));
  EXPECT_TRUE(
      JsonListEquals(static_cast<const google::protobuf::MessageLite&>(
                         *GeneratedParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb")),
                     static_cast<const google::protobuf::MessageLite&>(
                         *GeneratedParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb"))));
}

TEST_F(JsonEqualsTest, List_List_Generated_Dynamic) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb"))));
  EXPECT_TRUE(
      JsonListEquals(*GeneratedParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb"),
                     *DynamicParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb")));
  EXPECT_TRUE(
      JsonListEquals(static_cast<const google::protobuf::MessageLite&>(
                         *GeneratedParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb")),
                     static_cast<const google::protobuf::MessageLite&>(
                         *DynamicParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb"))));
}

TEST_F(JsonEqualsTest, List_List_Dynamic_Generated) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb"))));
  EXPECT_TRUE(
      JsonListEquals(*DynamicParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb"),
                     *GeneratedParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb")));
  EXPECT_TRUE(
      JsonListEquals(static_cast<const google::protobuf::MessageLite&>(
                         *DynamicParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb")),
                     static_cast<const google::protobuf::MessageLite&>(
                         *GeneratedParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb"))));
}

TEST_F(JsonEqualsTest, List_List_Dynamic_Dynamic) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(list_value: {
                                    values {}
                                    values { bool_value: true }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(list_value: {
                                        values {}
                                        values { bool_value: true }
                                      })pb"))));
  EXPECT_TRUE(
      JsonListEquals(*DynamicParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb"),
                     *DynamicParseTextProto<google::protobuf::ListValue>(
                         R"pb(
                           values {}
                           values { bool_value: true }
                         )pb")));
  EXPECT_TRUE(
      JsonListEquals(static_cast<const google::protobuf::MessageLite&>(
                         *DynamicParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb")),
                     static_cast<const google::protobuf::MessageLite&>(
                         *DynamicParseTextProto<google::protobuf::ListValue>(
                             R"pb(
                               values {}
                               values { bool_value: true }
                             )pb"))));
}

TEST_F(JsonEqualsTest, Map_Map_Generated_Generated) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb"))));
  EXPECT_TRUE(JsonMapEquals(*GeneratedParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb"),
                            *GeneratedParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb")));
  EXPECT_TRUE(
      JsonMapEquals(static_cast<const google::protobuf::MessageLite&>(
                        *GeneratedParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb")),
                    static_cast<const google::protobuf::MessageLite&>(
                        *GeneratedParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb"))));
}

TEST_F(JsonEqualsTest, Map_Map_Generated_Dynamic) {
  EXPECT_TRUE(JsonEquals(*GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb"))));
  EXPECT_TRUE(JsonMapEquals(*GeneratedParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb"),
                            *DynamicParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb")));
  EXPECT_TRUE(
      JsonMapEquals(static_cast<const google::protobuf::MessageLite&>(
                        *GeneratedParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb")),
                    static_cast<const google::protobuf::MessageLite&>(
                        *DynamicParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb"))));
}

TEST_F(JsonEqualsTest, Map_Map_Dynamic_Generated) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb"),
                         *GeneratedParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *GeneratedParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb"))));
  EXPECT_TRUE(JsonMapEquals(*DynamicParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb"),
                            *GeneratedParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb")));
  EXPECT_TRUE(
      JsonMapEquals(static_cast<const google::protobuf::MessageLite&>(
                        *DynamicParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb")),
                    static_cast<const google::protobuf::MessageLite&>(
                        *GeneratedParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb"))));
}

TEST_F(JsonEqualsTest, Map_Map_Dynamic_Dynamic) {
  EXPECT_TRUE(JsonEquals(*DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb"),
                         *DynamicParseTextProto<google::protobuf::Value>(
                             R"pb(struct_value: {
                                    fields {
                                      key: "foo"
                                      value: {}
                                    }
                                    fields {
                                      key: "bar"
                                      value: { bool_value: true }
                                    }
                                  })pb")));
  EXPECT_TRUE(JsonEquals(static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb")),
                         static_cast<const google::protobuf::MessageLite&>(
                             *DynamicParseTextProto<google::protobuf::Value>(
                                 R"pb(struct_value: {
                                        fields {
                                          key: "foo"
                                          value: {}
                                        }
                                        fields {
                                          key: "bar"
                                          value: { bool_value: true }
                                        }
                                      })pb"))));
  EXPECT_TRUE(JsonMapEquals(*DynamicParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb"),
                            *DynamicParseTextProto<google::protobuf::Struct>(
                                R"pb(
                                  fields {
                                    key: "foo"
                                    value: {}
                                  }
                                  fields {
                                    key: "bar"
                                    value: { bool_value: true }
                                  }
                                )pb")));
  EXPECT_TRUE(
      JsonMapEquals(static_cast<const google::protobuf::MessageLite&>(
                        *DynamicParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb")),
                    static_cast<const google::protobuf::MessageLite&>(
                        *DynamicParseTextProto<google::protobuf::Struct>(
                            R"pb(
                              fields {
                                key: "foo"
                                value: {}
                              }
                              fields {
                                key: "bar"
                                value: { bool_value: true }
                              }
                            )pb"))));
}

class ProtoJsonNativeJsonTest : public Test {
 public:
  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  template <typename T>
  auto GeneratedParseTextProto(absl::string_view text) {
    return ::cel::internal::GeneratedParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        arena(), text, descriptor_pool(), message_factory());
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(ProtoJsonNativeJsonTest, Null_Generated) {
  auto message = GeneratedParseTextProto<google::protobuf::Value>(
      R"pb(null_value: NULL_VALUE)pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(kJsonNull, other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Null_Dynamic) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(null_value: NULL_VALUE)pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(kJsonNull, other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Bool_Generated) {
  auto message = GeneratedParseTextProto<google::protobuf::Value>(
      R"pb(bool_value: true)pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonBool>(true)));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(true, other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Bool_Dynamic) {
  auto message =
      DynamicParseTextProto<google::protobuf::Value>(R"pb(bool_value: true)pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonBool>(true)));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(true, other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Number_Generated) {
  auto message = GeneratedParseTextProto<google::protobuf::Value>(
      R"pb(number_value: 1.0)pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonNumber>(1.0)));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(1.0, other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Number_Dynamic) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(number_value: 1.0)pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonNumber>(1.0)));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(1.0, other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, String_Generated) {
  auto message = GeneratedParseTextProto<google::protobuf::Value>(
      R"pb(string_value: "foo")pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonString>(JsonString("foo"))));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(JsonString("foo"), other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, String_Dynamic) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(string_value: "foo")pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonString>(JsonString("foo"))));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(JsonString("foo"), other_message), IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, List_Generated) {
  auto message = GeneratedParseTextProto<google::protobuf::Value>(
      R"pb(list_value: { values { bool_value: true } })pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonArray>(MakeJsonArray({true}))));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(MakeJsonArray({true}), other_message),
              IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, List_Dynamic) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(list_value: { values { bool_value: true } })pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonArray>(MakeJsonArray({true}))));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(MakeJsonArray({true}), other_message),
              IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Struct_Generated) {
  auto message = GeneratedParseTextProto<google::protobuf::Value>(
      R"pb(struct_value: {
             fields {
               key: "foo"
               value: { bool_value: true }
             }
           })pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonObject>(
                  MakeJsonObject({{JsonString("foo"), true}}))));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(MakeJsonObject({{JsonString("foo"), true}}),
                                    other_message),
              IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

TEST_F(ProtoJsonNativeJsonTest, Struct_Dynamic) {
  auto message = DynamicParseTextProto<google::protobuf::Value>(
      R"pb(struct_value: {
             fields {
               key: "foo"
               value: { bool_value: true }
             }
           })pb");
  EXPECT_THAT(ProtoJsonToNativeJson(*message),
              IsOkAndHolds(VariantWith<JsonObject>(
                  MakeJsonObject({{JsonString("foo"), true}}))));
  auto* other_message = message->New(arena());
  EXPECT_THAT(NativeJsonToProtoJson(MakeJsonObject({{JsonString("foo"), true}}),
                                    other_message),
              IsOk());
  EXPECT_THAT(*other_message, EqualsProto(*message));
}

}  // namespace
}  // namespace cel::internal
