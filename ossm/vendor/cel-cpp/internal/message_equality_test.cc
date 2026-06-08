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

#include "internal/message_equality.h"

#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "internal/message_type_name.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "internal/well_known_types.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::internal {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

google::protobuf::Arena* GetTestArena() {
  static absl::NoDestructor<google::protobuf::Arena> arena;
  return &*arena;
}

template <typename T>
google::protobuf::Message* ParseTextProto(absl::string_view text) {
  return DynamicParseTextProto<T>(GetTestArena(), text,
                                  GetTestingDescriptorPool(),
                                  GetTestingMessageFactory());
}

struct UnaryMessageEqualsTestParam {
  std::string name;
  std::vector<google::protobuf::Message*> ops;
  bool equal;
};

std::string UnaryMessageEqualsTestParamName(
    const TestParamInfo<UnaryMessageEqualsTestParam>& param_info) {
  return param_info.param.name;
}

using UnaryMessageEqualsTest = TestWithParam<UnaryMessageEqualsTestParam>;

google::protobuf::Message* PackMessage(const google::protobuf::Message& message) {
  const auto* descriptor =
      ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindMessageTypeByName(
          MessageTypeNameFor<google::protobuf::Any>()));
  const auto* prototype =
      ABSL_DIE_IF_NULL(GetTestingMessageFactory()->GetPrototype(descriptor));
  auto instance = prototype->New(GetTestArena());
  auto reflection = well_known_types::GetAnyReflectionOrDie(descriptor);
  reflection.SetTypeUrl(
      cel::to_address(instance),
      absl::StrCat("type.googleapis.com/", message.GetTypeName()));
  absl::Cord value;
  ABSL_CHECK(message.SerializeToString(&value));
  reflection.SetValue(cel::to_address(instance), value);
  return instance;
}

TEST_P(UnaryMessageEqualsTest, Equals) {
  const auto* pool = GetTestingDescriptorPool();
  auto* factory = GetTestingMessageFactory();
  const auto& test_case = GetParam();
  for (const auto& lhs : test_case.ops) {
    for (const auto& rhs : test_case.ops) {
      if (!test_case.equal && &lhs == &rhs) {
        continue;
      }
      EXPECT_THAT(MessageEquals(*lhs, *rhs, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs << " " << *rhs;
      EXPECT_THAT(MessageEquals(*rhs, *lhs, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs << " " << *rhs;
      // Test any.
      auto lhs_any = PackMessage(*lhs);
      auto rhs_any = PackMessage(*rhs);
      EXPECT_THAT(MessageEquals(*lhs_any, *rhs, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs_any << " " << *rhs;
      EXPECT_THAT(MessageEquals(*lhs, *rhs_any, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs << " " << *rhs_any;
      EXPECT_THAT(MessageEquals(*lhs_any, *rhs_any, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs_any << " " << *rhs_any;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    UnaryMessageEqualsTest, UnaryMessageEqualsTest,
    ValuesIn<UnaryMessageEqualsTestParam>({
        {
            .name = "NullValue_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::Value>(R"pb()pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(null_value: NULL_VALUE)pb"),
                },
            .equal = true,
        },
        {
            .name = "BoolValue_False_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::BoolValue>(R"pb()pb"),
                    ParseTextProto<google::protobuf::BoolValue>(
                        R"pb(value: false)pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(bool_value: false)pb"),
                },
            .equal = true,
        },
        {
            .name = "BoolValue_True_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::BoolValue>(
                        R"pb(value: true)pb"),
                    ParseTextProto<google::protobuf::Value>(R"pb(bool_value:
                                                                     true)pb"),
                },
            .equal = true,
        },
        {
            .name = "StringValue_Empty_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::StringValue>(R"pb()pb"),
                    ParseTextProto<google::protobuf::StringValue>(
                        R"pb(value: "")pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(string_value: "")pb"),
                },
            .equal = true,
        },
        {
            .name = "StringValue_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::StringValue>(
                        R"pb(value: "foo")pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(string_value: "foo")pb"),
                },
            .equal = true,
        },
        {
            .name = "BytesValue_Empty_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::BytesValue>(R"pb()pb"),
                    ParseTextProto<google::protobuf::BytesValue>(
                        R"pb(value: "")pb"),
                },
            .equal = true,
        },
        {
            .name = "BytesValue_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::BytesValue>(
                        R"pb(value: "foo")pb"),
                    ParseTextProto<google::protobuf::BytesValue>(
                        R"pb(value: "foo")pb"),
                },
            .equal = true,
        },
        {
            .name = "ListValue_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(list_value: { values { bool_value: true } })pb"),
                    ParseTextProto<google::protobuf::ListValue>(
                        R"pb(values { bool_value: true })pb"),
                },
            .equal = true,
        },
        {
            .name = "ListValue_NotEqual",
            .ops =
                {
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(list_value: { values { number_value: 0.0 } })pb"),
                    ParseTextProto<google::protobuf::ListValue>(
                        R"pb(values { number_value: 1.0 })pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(list_value: { values { number_value: 2.0 } })pb"),
                    ParseTextProto<google::protobuf::ListValue>(
                        R"pb(values { number_value: 3.0 })pb"),
                },
            .equal = false,
        },
        {
            .name = "StructValue_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(struct_value: {
                               fields {
                                 key: "foo"
                                 value: { bool_value: true }
                               }
                             })pb"),
                    ParseTextProto<google::protobuf::Struct>(
                        R"pb(fields {
                               key: "foo"
                               value: { bool_value: true }
                             })pb"),
                },
            .equal = true,
        },
        {
            .name = "StructValue_NotEqual",
            .ops =
                {
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(struct_value: {
                               fields {
                                 key: "foo"
                                 value: { number_value: 0.0 }
                               }
                             })pb"),
                    ParseTextProto<google::protobuf::Struct>(
                        R"pb(
                          fields {
                            key: "bar"
                            value: { number_value: 0.0 }
                          })pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(struct_value: {
                               fields {
                                 key: "foo"
                                 value: { number_value: 1.0 }
                               }
                             })pb"),
                    ParseTextProto<google::protobuf::Struct>(
                        R"pb(
                          fields {
                            key: "bar"
                            value: { number_value: 1.0 }
                          })pb"),
                },
            .equal = false,
        },
        {
            .name = "Heterogeneous_Equal",
            .ops =
                {
                    ParseTextProto<google::protobuf::Int32Value>(R"pb()pb"),
                    ParseTextProto<google::protobuf::Int64Value>(R"pb()pb"),
                    ParseTextProto<google::protobuf::UInt32Value>(R"pb()pb"),
                    ParseTextProto<google::protobuf::UInt64Value>(R"pb()pb"),
                    ParseTextProto<google::protobuf::FloatValue>(R"pb()pb"),
                    ParseTextProto<google::protobuf::DoubleValue>(R"pb()pb"),
                    ParseTextProto<google::protobuf::Value>(R"pb(number_value:
                                                                     0.0)pb"),
                },
            .equal = true,
        },
        {
            .name = "Message_Equals",
            .ops =
                {
                    ParseTextProto<TestAllTypesProto3>(R"pb()pb"),
                    ParseTextProto<TestAllTypesProto3>(R"pb()pb"),
                },
            .equal = true,
        },
        {
            .name = "Heterogeneous_NotEqual",
            .ops =
                {
                    ParseTextProto<google::protobuf::BoolValue>(
                        R"pb(value: false)pb"),
                    ParseTextProto<google::protobuf::Int32Value>(
                        R"pb(value: 0)pb"),
                    ParseTextProto<google::protobuf::Int64Value>(
                        R"pb(value: 1)pb"),
                    ParseTextProto<google::protobuf::UInt32Value>(
                        R"pb(value: 2)pb"),
                    ParseTextProto<google::protobuf::UInt64Value>(
                        R"pb(value: 3)pb"),
                    ParseTextProto<google::protobuf::FloatValue>(
                        R"pb(value: 4.0)pb"),
                    ParseTextProto<google::protobuf::DoubleValue>(
                        R"pb(value: 5.0)pb"),
                    ParseTextProto<google::protobuf::Value>(R"pb()pb"),
                    ParseTextProto<google::protobuf::Value>(R"pb(bool_value:
                                                                     true)pb"),
                    ParseTextProto<google::protobuf::Value>(R"pb(number_value:
                                                                     6.0)pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(string_value: "bar")pb"),
                    ParseTextProto<google::protobuf::BytesValue>(
                        R"pb(value: "foo")pb"),
                    ParseTextProto<google::protobuf::StringValue>(
                        R"pb(value: "")pb"),
                    ParseTextProto<google::protobuf::StringValue>(
                        R"pb(value: "foo")pb"),
                    ParseTextProto<google::protobuf::Value>(
                        R"pb(list_value: {})pb"),
                    ParseTextProto<google::protobuf::ListValue>(
                        R"pb(values { bool_value: true })pb"),
                    ParseTextProto<google::protobuf::Value>(R"pb(struct_value:
                                                                     {})pb"),
                    ParseTextProto<google::protobuf::Struct>(
                        R"pb(fields {
                               key: "foo"
                               value: { bool_value: false }
                             })pb"),
                    ParseTextProto<google::protobuf::Duration>(R"pb()pb"),
                    ParseTextProto<google::protobuf::Duration>(
                        R"pb(seconds: 1 nanos: 1)pb"),
                    ParseTextProto<google::protobuf::Timestamp>(R"pb()pb"),
                    ParseTextProto<google::protobuf::Timestamp>(
                        R"pb(seconds: 1 nanos: 1)pb"),
                    ParseTextProto<TestAllTypesProto3>(R"pb()pb"),
                    ParseTextProto<TestAllTypesProto3>(
                        R"pb(single_bool: true)pb"),
                },
            .equal = false,
        },
    }),
    UnaryMessageEqualsTestParamName);

struct UnaryMessageFieldEqualsTestParam {
  std::string name;
  std::string message;
  std::vector<std::string> fields;
  bool equal;
};

std::string UnaryMessageFieldEqualsTestParamName(
    const TestParamInfo<UnaryMessageFieldEqualsTestParam>& param_info) {
  return param_info.param.name;
}

using UnaryMessageFieldEqualsTest =
    TestWithParam<UnaryMessageFieldEqualsTestParam>;

void PackMessageTo(const google::protobuf::Message& message, google::protobuf::Message* instance) {
  auto reflection =
      *well_known_types::GetAnyReflection(instance->GetDescriptor());
  reflection.SetTypeUrl(
      instance, absl::StrCat("type.googleapis.com/", message.GetTypeName()));
  absl::Cord value;
  ABSL_CHECK(message.SerializeToString(&value));
  reflection.SetValue(instance, value);
}

absl::optional<std::pair<Owned<google::protobuf::Message>,
                         const google::protobuf::FieldDescriptor* absl_nonnull>>
PackTestAllTypesProto3Field(const google::protobuf::Message& message,
                            const google::protobuf::FieldDescriptor* absl_nonnull field) {
  if (field->is_map()) {
    return absl::nullopt;
  }
  if (field->is_repeated() &&
      field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
    const auto* descriptor = message.GetDescriptor();
    const auto* any_field = descriptor->FindFieldByName("repeated_any");
    auto packed = WrapShared(message.New(), NewDeleteAllocator<>{});
    const int size = message.GetReflection()->FieldSize(message, field);
    for (int i = 0; i < size; ++i) {
      PackMessageTo(
          message.GetReflection()->GetRepeatedMessage(message, field, i),
          packed->GetReflection()->AddMessage(cel::to_address(packed),
                                              any_field));
    }
    return std::pair{packed, any_field};
  }
  if (!field->is_repeated() &&
      field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
    const auto* descriptor = message.GetDescriptor();
    const auto* any_field = descriptor->FindFieldByName("single_any");
    auto packed = WrapShared(message.New(), NewDeleteAllocator<>{});
    PackMessageTo(message.GetReflection()->GetMessage(message, field),
                  packed->GetReflection()->MutableMessage(
                      cel::to_address(packed), any_field));
    return std::pair{packed, any_field};
  }
  return absl::nullopt;
}

TEST_P(UnaryMessageFieldEqualsTest, Equals) {
  // We perform exhaustive comparison by testing for equality (or inequality)
  // against all combinations of fields. Additionally we convert to
  // `google.protobuf.Any` where applicable. This is all done for coverage and
  // to ensure different combinations, regardless of argument order, produce the
  // same result.

  const auto* pool = GetTestingDescriptorPool();
  auto* factory = GetTestingMessageFactory();
  const auto& test_case = GetParam();
  auto lhs_message = ParseTextProto<TestAllTypesProto3>(test_case.message);
  auto rhs_message = ParseTextProto<TestAllTypesProto3>(test_case.message);
  const auto* descriptor = ABSL_DIE_IF_NULL(
      pool->FindMessageTypeByName(MessageTypeNameFor<TestAllTypesProto3>()));
  for (const auto& lhs : test_case.fields) {
    for (const auto& rhs : test_case.fields) {
      if (!test_case.equal && lhs == rhs) {
        // When testing for inequality, do not compare the same field to itself.
        continue;
      }
      const auto* lhs_field =
          ABSL_DIE_IF_NULL(descriptor->FindFieldByName(lhs));
      const auto* rhs_field =
          ABSL_DIE_IF_NULL(descriptor->FindFieldByName(rhs));
      EXPECT_THAT(MessageFieldEquals(*lhs_message, lhs_field, *rhs_message,
                                     rhs_field, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs_message << " " << lhs_field->name() << " " << *rhs_message
          << " " << rhs_field->name();
      EXPECT_THAT(MessageFieldEquals(*rhs_message, rhs_field, *lhs_message,
                                     lhs_field, pool, factory),
                  IsOkAndHolds(test_case.equal))
          << *lhs_message << " " << lhs_field->name() << " " << *rhs_message
          << " " << rhs_field->name();
      if (!lhs_field->is_repeated() &&
          lhs_field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        EXPECT_THAT(MessageFieldEquals(lhs_message->GetReflection()->GetMessage(
                                           *lhs_message, lhs_field),
                                       *rhs_message, rhs_field, pool, factory),
                    IsOkAndHolds(test_case.equal))
            << *lhs_message << " " << lhs_field->name() << " " << *rhs_message
            << " " << rhs_field->name();
        EXPECT_THAT(MessageFieldEquals(*rhs_message, rhs_field,
                                       lhs_message->GetReflection()->GetMessage(
                                           *lhs_message, lhs_field),
                                       pool, factory),
                    IsOkAndHolds(test_case.equal))
            << *lhs_message << " " << lhs_field->name() << " " << *rhs_message
            << " " << rhs_field->name();
      }
      if (!rhs_field->is_repeated() &&
          rhs_field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        EXPECT_THAT(MessageFieldEquals(*lhs_message, lhs_field,
                                       rhs_message->GetReflection()->GetMessage(
                                           *rhs_message, rhs_field),
                                       pool, factory),
                    IsOkAndHolds(test_case.equal))
            << *lhs_message << " " << lhs_field->name() << " " << *rhs_message
            << " " << rhs_field->name();
        EXPECT_THAT(MessageFieldEquals(rhs_message->GetReflection()->GetMessage(
                                           *rhs_message, rhs_field),
                                       *lhs_message, lhs_field, pool, factory),
                    IsOkAndHolds(test_case.equal))
            << *lhs_message << " " << lhs_field->name() << " " << *rhs_message
            << " " << rhs_field->name();
      }
      // Test `google.protobuf.Any`.
      absl::optional<std::pair<Owned<google::protobuf::Message>,
                               const google::protobuf::FieldDescriptor* absl_nonnull>>
          lhs_any = PackTestAllTypesProto3Field(*lhs_message, lhs_field);
      absl::optional<std::pair<Owned<google::protobuf::Message>,
                               const google::protobuf::FieldDescriptor* absl_nonnull>>
          rhs_any = PackTestAllTypesProto3Field(*rhs_message, rhs_field);
      if (lhs_any) {
        EXPECT_THAT(MessageFieldEquals(*lhs_any->first, lhs_any->second,
                                       *rhs_message, rhs_field, pool, factory),
                    IsOkAndHolds(test_case.equal))
            << *lhs_any->first << " " << *rhs_message;
        if (!lhs_any->second->is_repeated()) {
          EXPECT_THAT(
              MessageFieldEquals(lhs_any->first->GetReflection()->GetMessage(
                                     *lhs_any->first, lhs_any->second),
                                 *rhs_message, rhs_field, pool, factory),
              IsOkAndHolds(test_case.equal))
              << *lhs_any->first << " " << *rhs_message;
        }
      }
      if (rhs_any) {
        EXPECT_THAT(MessageFieldEquals(*lhs_message, lhs_field, *rhs_any->first,
                                       rhs_any->second, pool, factory),
                    IsOkAndHolds(test_case.equal))
            << *lhs_message << " " << *rhs_any->first;
        if (!rhs_any->second->is_repeated()) {
          EXPECT_THAT(
              MessageFieldEquals(*lhs_message, lhs_field,
                                 rhs_any->first->GetReflection()->GetMessage(
                                     *rhs_any->first, rhs_any->second),
                                 pool, factory),
              IsOkAndHolds(test_case.equal))
              << *lhs_message << " " << *rhs_any->first;
        }
      }
      if (lhs_any && rhs_any) {
        EXPECT_THAT(
            MessageFieldEquals(*lhs_any->first, lhs_any->second,
                               *rhs_any->first, rhs_any->second, pool, factory),
            IsOkAndHolds(test_case.equal))
            << *lhs_any->first << " " << *rhs_any->second;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    UnaryMessageFieldEqualsTest, UnaryMessageFieldEqualsTest,
    ValuesIn<UnaryMessageFieldEqualsTestParam>({
        {
            .name = "Heterogeneous_Single_Equal",
            .message = R"pb(
              single_int32: 1
              single_int64: 1
              single_uint32: 1
              single_uint64: 1
              single_float: 1
              single_double: 1
              single_value: { number_value: 1 }
              single_int32_wrapper: { value: 1 }
              single_int64_wrapper: { value: 1 }
              single_uint32_wrapper: { value: 1 }
              single_uint64_wrapper: { value: 1 }
              single_float_wrapper: { value: 1 }
              single_double_wrapper: { value: 1 }
              standalone_enum: BAR
            )pb",
            .fields =
                {
                    "single_int32",
                    "single_int64",
                    "single_uint32",
                    "single_uint64",
                    "single_float",
                    "single_double",
                    "single_value",
                    "single_int32_wrapper",
                    "single_int64_wrapper",
                    "single_uint32_wrapper",
                    "single_uint64_wrapper",
                    "single_float_wrapper",
                    "single_double_wrapper",
                    "standalone_enum",
                },
            .equal = true,
        },
        {
            .name = "Heterogeneous_Single_NotEqual",
            .message = R"pb(
              null_value: NULL_VALUE
              single_bool: false
              single_int32: 2
              single_int64: 3
              single_uint32: 4
              single_uint64: 5
              single_float: NaN
              single_double: NaN
              single_string: "foo"
              single_bytes: "foo"
              single_value: { number_value: 8 }
              single_int32_wrapper: { value: 9 }
              single_int64_wrapper: { value: 10 }
              single_uint32_wrapper: { value: 11 }
              single_uint64_wrapper: { value: 12 }
              single_float_wrapper: { value: 13 }
              single_double_wrapper: { value: 14 }
              single_string_wrapper: { value: "bar" }
              single_bytes_wrapper: { value: "bar" }
              standalone_enum: BAR
            )pb",
            .fields =
                {
                    "null_value",
                    "single_bool",
                    "single_int32",
                    "single_int64",
                    "single_uint32",
                    "single_uint64",
                    "single_float",
                    "single_double",
                    "single_string",
                    "single_bytes",
                    "single_value",
                    "single_int32_wrapper",
                    "single_int64_wrapper",
                    "single_uint32_wrapper",
                    "single_uint64_wrapper",
                    "single_float_wrapper",
                    "single_double_wrapper",
                    "standalone_enum",
                },
            .equal = false,
        },
        {
            .name = "Heterogeneous_Repeated_Equal",
            .message = R"pb(
              repeated_int32: 1
              repeated_int64: 1
              repeated_uint32: 1
              repeated_uint64: 1
              repeated_float: 1
              repeated_double: 1
              repeated_value: { number_value: 1 }
              repeated_int32_wrapper: { value: 1 }
              repeated_int64_wrapper: { value: 1 }
              repeated_uint32_wrapper: { value: 1 }
              repeated_uint64_wrapper: { value: 1 }
              repeated_float_wrapper: { value: 1 }
              repeated_double_wrapper: { value: 1 }
              repeated_nested_enum: BAR
              single_value: { list_value: { values { number_value: 1 } } }
              list_value: { values { number_value: 1 } }
            )pb",
            .fields =
                {
                    "repeated_int32",
                    "repeated_int64",
                    "repeated_uint32",
                    "repeated_uint64",
                    "repeated_float",
                    "repeated_double",
                    "repeated_value",
                    "repeated_int32_wrapper",
                    "repeated_int64_wrapper",
                    "repeated_uint32_wrapper",
                    "repeated_uint64_wrapper",
                    "repeated_float_wrapper",
                    "repeated_double_wrapper",
                    "repeated_nested_enum",
                    "single_value",
                    "list_value",
                },
            .equal = true,
        },
        {
            .name = "Heterogeneous_Repeated_NotEqual",
            .message = R"pb(
              repeated_null_value: NULL_VALUE
              repeated_bool: false
              repeated_int32: 2
              repeated_int64: 3
              repeated_uint32: 4
              repeated_uint64: 5
              repeated_float: 6
              repeated_double: 7
              repeated_string: "foo"
              repeated_bytes: "foo"
              repeated_value: { number_value: 8 }
              repeated_int32_wrapper: { value: 9 }
              repeated_int64_wrapper: { value: 10 }
              repeated_uint32_wrapper: { value: 11 }
              repeated_uint64_wrapper: { value: 12 }
              repeated_float_wrapper: { value: 13 }
              repeated_double_wrapper: { value: 14 }
              repeated_string_wrapper: { value: "bar" }
              repeated_bytes_wrapper: { value: "bar" }
              repeated_nested_enum: BAR
            )pb",
            .fields =
                {
                    "repeated_null_value",
                    "repeated_bool",
                    "repeated_int32",
                    "repeated_int64",
                    "repeated_uint32",
                    "repeated_uint64",
                    "repeated_float",
                    "repeated_double",
                    "repeated_string",
                    "repeated_bytes",
                    "repeated_value",
                    "repeated_int32_wrapper",
                    "repeated_int64_wrapper",
                    "repeated_uint32_wrapper",
                    "repeated_uint64_wrapper",
                    "repeated_float_wrapper",
                    "repeated_double_wrapper",
                    "repeated_nested_enum",
                },
            .equal = false,
        },
        {
            .name = "Heterogeneous_Map_Equal",
            .message = R"pb(
              map_int32_int32 { key: 1 value: 1 }
              map_int32_uint32 { key: 1 value: 1 }
              map_int32_int64 { key: 1 value: 1 }
              map_int32_uint64 { key: 1 value: 1 }
              map_int32_float { key: 1 value: 1 }
              map_int32_double { key: 1 value: 1 }
              map_int32_enum { key: 1 value: BAR }
              map_int32_value {
                key: 1
                value: { number_value: 1 }
              }
              map_int32_int32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int32_uint32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int32_int64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int32_uint64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int32_float_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int32_double_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int64_int32 { key: 1 value: 1 }
              map_int64_uint32 { key: 1 value: 1 }
              map_int64_int64 { key: 1 value: 1 }
              map_int64_uint64 { key: 1 value: 1 }
              map_int64_float { key: 1 value: 1 }
              map_int64_double { key: 1 value: 1 }
              map_int64_enum { key: 1 value: BAR }
              map_int64_value {
                key: 1
                value: { number_value: 1 }
              }
              map_int64_int32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int64_uint32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int64_int64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int64_uint64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int64_float_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_int64_double_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint32_int32 { key: 1 value: 1 }
              map_uint32_uint32 { key: 1 value: 1 }
              map_uint32_int64 { key: 1 value: 1 }
              map_uint32_uint64 { key: 1 value: 1 }
              map_uint32_float { key: 1 value: 1 }
              map_uint32_double { key: 1 value: 1 }
              map_uint32_enum { key: 1 value: BAR }
              map_uint32_value {
                key: 1
                value: { number_value: 1 }
              }
              map_uint32_int32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint32_uint32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint32_int64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint32_uint64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint32_float_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint32_double_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint64_int32 { key: 1 value: 1 }
              map_uint64_uint32 { key: 1 value: 1 }
              map_uint64_int64 { key: 1 value: 1 }
              map_uint64_uint64 { key: 1 value: 1 }
              map_uint64_float { key: 1 value: 1 }
              map_uint64_double { key: 1 value: 1 }
              map_uint64_enum { key: 1 value: BAR }
              map_uint64_value {
                key: 1
                value: { number_value: 1 }
              }
              map_uint64_int32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint64_uint32_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint64_int64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint64_uint64_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint64_float_wrapper {
                key: 1
                value: { value: 1 }
              }
              map_uint64_double_wrapper {
                key: 1
                value: { value: 1 }
              }
            )pb",
            .fields =
                {
                    "map_int32_int32",          "map_int32_uint32",
                    "map_int32_int64",          "map_int32_uint64",
                    "map_int32_float",          "map_int32_double",
                    "map_int32_enum",           "map_int32_value",
                    "map_int32_int32_wrapper",  "map_int32_uint32_wrapper",
                    "map_int32_int64_wrapper",  "map_int32_uint64_wrapper",
                    "map_int32_float_wrapper",  "map_int32_double_wrapper",
                    "map_int64_int32",          "map_int64_uint32",
                    "map_int64_int64",          "map_int64_uint64",
                    "map_int64_float",          "map_int64_double",
                    "map_int64_enum",           "map_int64_value",
                    "map_int64_int32_wrapper",  "map_int64_uint32_wrapper",
                    "map_int64_int64_wrapper",  "map_int64_uint64_wrapper",
                    "map_int64_float_wrapper",  "map_int64_double_wrapper",
                    "map_uint32_int32",         "map_uint32_uint32",
                    "map_uint32_int64",         "map_uint32_uint64",
                    "map_uint32_float",         "map_uint32_double",
                    "map_uint32_enum",          "map_uint32_value",
                    "map_uint32_int32_wrapper", "map_uint32_uint32_wrapper",
                    "map_uint32_int64_wrapper", "map_uint32_uint64_wrapper",
                    "map_uint32_float_wrapper", "map_uint32_double_wrapper",
                    "map_uint64_int32",         "map_uint64_uint32",
                    "map_uint64_int64",         "map_uint64_uint64",
                    "map_uint64_float",         "map_uint64_double",
                    "map_uint64_enum",          "map_uint64_value",
                    "map_uint64_int32_wrapper", "map_uint64_uint32_wrapper",
                    "map_uint64_int64_wrapper", "map_uint64_uint64_wrapper",
                    "map_uint64_float_wrapper", "map_uint64_double_wrapper",
                },
            .equal = true,
        },
        {
            .name = "Heterogeneous_Map_NotEqual",
            .message = R"pb(
              map_bool_bool { key: false value: false }
              map_bool_int32 { key: false value: 1 }
              map_bool_uint32 { key: false value: 0 }
              map_int32_int32 { key: 0x7FFFFFFF value: 1 }
              map_int64_int64 { key: 0x7FFFFFFFFFFFFFFF value: 1 }
              map_uint32_uint32 { key: 0xFFFFFFFF value: 1 }
              map_uint64_uint64 { key: 0xFFFFFFFFFFFFFFFF value: 1 }
              map_string_string { key: "foo" value: "bar" }
              map_string_bytes { key: "foo" value: "bar" }
              map_int32_bytes { key: -2147483648 value: "bar" }
              map_int64_bytes { key: -9223372036854775808 value: "bar" }
              map_int32_float { key: -2147483648 value: 1 }
              map_int64_double { key: -9223372036854775808 value: 1 }
              map_uint32_string { key: 0xFFFFFFFF value: "bar" }
              map_uint64_string { key: 0xFFFFFFFF value: "foo" }
              map_uint32_bytes { key: 0xFFFFFFFF value: "bar" }
              map_uint64_bytes { key: 0xFFFFFFFF value: "foo" }
              map_uint32_bool { key: 0xFFFFFFFF value: false }
              map_uint64_bool { key: 0xFFFFFFFF value: true }
              single_value: {
                struct_value: {
                  fields {
                    key: "bar"
                    value: { string_value: "foo" }
                  }
                }
              }
              single_struct: {
                fields {
                  key: "baz"
                  value: { string_value: "foo" }
                }
              }
              standalone_message: {}
            )pb",
            .fields =
                {
                    "map_bool_bool",     "map_bool_int32",
                    "map_bool_uint32",   "map_int32_int32",
                    "map_int64_int64",   "map_uint32_uint32",
                    "map_uint64_uint64", "map_string_string",
                    "map_string_bytes",  "map_int32_bytes",
                    "map_int64_bytes",   "map_int32_float",
                    "map_int64_double",  "map_uint32_string",
                    "map_uint64_string", "map_uint32_bytes",
                    "map_uint64_bytes",  "map_uint32_bool",
                    "map_uint64_bool",   "single_value",
                    "single_struct",     "standalone_message",
                },
            .equal = false,
        },
    }),
    UnaryMessageFieldEqualsTestParamName);

TEST(MessageEquals, AnyFallback) {
  const auto* pool = GetTestingDescriptorPool();
  auto* factory = GetTestingMessageFactory();
  google::protobuf::Arena arena;
  auto message1 = DynamicParseTextProto<TestAllTypesProto3>(
      &arena, R"pb(single_any: {
                     type_url: "type.googleapis.com/message.that.does.not.Exist"
                     value: "foo"
                   })pb",
      pool, factory);
  auto message2 = DynamicParseTextProto<TestAllTypesProto3>(
      &arena, R"pb(single_any: {
                     type_url: "type.googleapis.com/message.that.does.not.Exist"
                     value: "foo"
                   })pb",
      pool, factory);
  auto message3 = DynamicParseTextProto<TestAllTypesProto3>(
      &arena, R"pb(single_any: {
                     type_url: "type.googleapis.com/message.that.does.not.Exist"
                     value: "bar"
                   })pb",
      pool, factory);
  EXPECT_THAT(MessageEquals(*message1, *message2, pool, factory),
              IsOkAndHolds(IsTrue()));
  EXPECT_THAT(MessageEquals(*message2, *message1, pool, factory),
              IsOkAndHolds(IsTrue()));
  EXPECT_THAT(MessageEquals(*message1, *message3, pool, factory),
              IsOkAndHolds(IsFalse()));
  EXPECT_THAT(MessageEquals(*message3, *message1, pool, factory),
              IsOkAndHolds(IsFalse()));
}

TEST(MessageFieldEquals, AnyFallback) {
  const auto* pool = GetTestingDescriptorPool();
  auto* factory = GetTestingMessageFactory();
  google::protobuf::Arena arena;
  auto message1 = DynamicParseTextProto<TestAllTypesProto3>(
      &arena, R"pb(single_any: {
                     type_url: "type.googleapis.com/message.that.does.not.Exist"
                     value: "foo"
                   })pb",
      pool, factory);
  auto message2 = DynamicParseTextProto<TestAllTypesProto3>(
      &arena, R"pb(single_any: {
                     type_url: "type.googleapis.com/message.that.does.not.Exist"
                     value: "foo"
                   })pb",
      pool, factory);
  auto message3 = DynamicParseTextProto<TestAllTypesProto3>(
      &arena, R"pb(single_any: {
                     type_url: "type.googleapis.com/message.that.does.not.Exist"
                     value: "bar"
                   })pb",
      pool, factory);
  EXPECT_THAT(MessageFieldEquals(
                  *message1,
                  ABSL_DIE_IF_NULL(
                      message1->GetDescriptor()->FindFieldByName("single_any")),
                  *message2,
                  ABSL_DIE_IF_NULL(
                      message2->GetDescriptor()->FindFieldByName("single_any")),
                  pool, factory),
              IsOkAndHolds(IsTrue()));
  EXPECT_THAT(MessageFieldEquals(
                  *message2,
                  ABSL_DIE_IF_NULL(
                      message2->GetDescriptor()->FindFieldByName("single_any")),
                  *message1,
                  ABSL_DIE_IF_NULL(
                      message1->GetDescriptor()->FindFieldByName("single_any")),
                  pool, factory),
              IsOkAndHolds(IsTrue()));
  EXPECT_THAT(MessageFieldEquals(
                  *message1,
                  ABSL_DIE_IF_NULL(
                      message1->GetDescriptor()->FindFieldByName("single_any")),
                  *message3,
                  ABSL_DIE_IF_NULL(
                      message3->GetDescriptor()->FindFieldByName("single_any")),
                  pool, factory),
              IsOkAndHolds(IsFalse()));
  EXPECT_THAT(MessageFieldEquals(
                  *message3,
                  ABSL_DIE_IF_NULL(
                      message3->GetDescriptor()->FindFieldByName("single_any")),
                  *message1,
                  ABSL_DIE_IF_NULL(
                      message1->GetDescriptor()->FindFieldByName("single_any")),
                  pool, factory),
              IsOkAndHolds(IsFalse()));
}

}  // namespace
}  // namespace cel::internal
