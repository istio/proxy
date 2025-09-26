// Copyright 2025 Google LLC
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

#include "common/type_proto.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::internal::test::EqualsProto;

enum class RoundTrip {
  kYes,
  kNo,
};

struct TestCase {
  std::string type_pb;
  absl::StatusOr<TypeKind> type_kind;
  RoundTrip round_trip = RoundTrip::kYes;
};

class TypeFromProtoTest : public ::testing::TestWithParam<TestCase> {};

TEST_P(TypeFromProtoTest, FromProtoWorks) {
  const google::protobuf::DescriptorPool* descriptor_pool =
      internal::GetTestingDescriptorPool();
  google::protobuf::Arena arena;

  const TestCase& test_case = GetParam();
  cel::expr::Type type_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(test_case.type_pb, &type_pb));
  absl::StatusOr<Type> result = TypeFromProto(type_pb, descriptor_pool, &arena);

  if (test_case.type_kind.ok()) {
    ASSERT_OK_AND_ASSIGN(Type type, result);

    EXPECT_EQ(type.kind(), *test_case.type_kind)
        << absl::StrCat("got: ", type.DebugString(),
                        " want: ", TypeKindToString(*test_case.type_kind));
  } else {
    EXPECT_THAT(result, StatusIs(test_case.type_kind.status().code()));
  }
}

TEST_P(TypeFromProtoTest, RoundTripProtoWorks) {
  const google::protobuf::DescriptorPool* descriptor_pool =
      internal::GetTestingDescriptorPool();
  google::protobuf::Arena arena;

  const TestCase& test_case = GetParam();
  if (!test_case.type_kind.ok() || test_case.round_trip == RoundTrip::kNo) {
    return GTEST_SUCCEED();
  }
  cel::expr::Type type_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(test_case.type_pb, &type_pb));
  absl::StatusOr<Type> result = TypeFromProto(type_pb, descriptor_pool, &arena);

  ASSERT_THAT(test_case.type_kind, IsOk());
  ASSERT_OK_AND_ASSIGN(Type type, result);

  EXPECT_EQ(type.kind(), *test_case.type_kind)
      << absl::StrCat("got: ", type.DebugString(),
                      " want: ", TypeKindToString(*test_case.type_kind));
  cel::expr::Type round_trip_pb;
  ASSERT_THAT(TypeToProto(type, &round_trip_pb), IsOk());
  EXPECT_THAT(round_trip_pb, EqualsProto(type_pb));
}

INSTANTIATE_TEST_SUITE_P(
    TypeFromProtoTest, TypeFromProtoTest,
    testing::Values<TestCase>(
        TestCase{
            R"pb(
              abstract_type {
                name: "foo"
                parameter_types { primitive: INT64 }
                parameter_types { primitive: STRING }
              }
            )pb",
            TypeKind::kOpaque},
        TestCase{R"pb(
                   dyn {}
                 )pb",
                 TypeKind::kDyn},
        TestCase{R"pb(
                   error {}
                 )pb",
                 TypeKind::kError},
        TestCase{R"pb(
                   list_type { elem_type { primitive: INT64 } }
                 )pb",
                 TypeKind::kList},
        TestCase{R"pb(
                   map_type {
                     key_type { primitive: INT64 }
                     value_type { primitive: STRING }
                   }
                 )pb",
                 TypeKind::kMap},
        TestCase{R"pb(
                   message_type: "google.api.expr.runtime.TestExtensions"
                 )pb",
                 TypeKind::kMessage},
        TestCase{R"pb(
                   message_type: "com.example.UnknownMessage"
                 )pb",
                 absl::InvalidArgumentError("")},
        // Special-case well known types referenced by
        // equivalent proto message types.
        TestCase{R"pb(
                   message_type: "google.protobuf.Any"
                 )pb",
                 TypeKind::kAny, RoundTrip::kNo},
        TestCase{R"pb(
                   message_type: "google.protobuf.Timestamp"
                 )pb",
                 TypeKind::kTimestamp, RoundTrip::kNo},
        TestCase{R"pb(
                   message_type: "google.protobuf.Duration"
                 )pb",
                 TypeKind::kDuration, RoundTrip::kNo},
        TestCase{R"pb(
                   message_type: "google.protobuf.Struct"
                 )pb",
                 TypeKind::kMap, RoundTrip::kNo},
        TestCase{R"pb(
                   message_type: "google.protobuf.ListValue"
                 )pb",
                 TypeKind::kList, RoundTrip::kNo},
        TestCase{R"pb(
                   message_type: "google.protobuf.Value"
                 )pb",
                 TypeKind::kDyn, RoundTrip::kNo},
        TestCase{R"pb(
                   message_type: "google.protobuf.Int64Value"
                 )pb",
                 TypeKind::kIntWrapper, RoundTrip::kNo},
        TestCase{R"pb(
                   null: 0
                 )pb",
                 TypeKind::kNull},
        TestCase{
            R"pb(
              primitive: BOOL)pb",
            TypeKind::kBool},
        TestCase{
            R"pb(
              primitive: BYTES)pb",
            TypeKind::kBytes},
        TestCase{
            R"pb(
              primitive: DOUBLE)pb",
            TypeKind::kDouble},
        TestCase{
            R"pb(
              primitive: INT64)pb",
            TypeKind::kInt},
        TestCase{
            R"pb(
              primitive: STRING)pb",
            TypeKind::kString},
        TestCase{
            R"pb(
              primitive: UINT64)pb",
            TypeKind::kUint},
        TestCase{
            R"pb(
              primitive: PRIMITIVE_TYPE_UNSPECIFIED)pb",
            absl::InvalidArgumentError("")},
        TestCase{
            R"pb(
              type { type { primitive: UINT64 } })pb",
            TypeKind::kType},
        TestCase{
            R"pb(
              type_param: "T")pb",
            TypeKind::kTypeParam},
        TestCase{
            R"pb(
              well_known: ANY)pb",
            TypeKind::kAny},
        TestCase{
            R"pb(
              well_known: TIMESTAMP)pb",
            TypeKind::kTimestamp},
        TestCase{
            R"pb(
              well_known: DURATION)pb",
            TypeKind::kDuration},
        TestCase{
            R"pb(
              well_known: WELL_KNOWN_TYPE_UNSPECIFIED)pb",
            absl::InvalidArgumentError("")},
        TestCase{
            R"pb(
              wrapper: BOOL
            )pb",
            TypeKind::kBoolWrapper},
        TestCase{
            R"pb(
              wrapper: BYTES
            )pb",
            TypeKind::kBytesWrapper},
        TestCase{
            R"pb(
              wrapper: DOUBLE
            )pb",
            TypeKind::kDoubleWrapper},
        TestCase{
            R"pb(
              wrapper: INT64
            )pb",
            TypeKind::kIntWrapper},
        TestCase{
            R"pb(
              wrapper: STRING
            )pb",
            TypeKind::kStringWrapper},
        TestCase{
            R"pb(
              wrapper: UINT64
            )pb",
            TypeKind::kUintWrapper},
        TestCase{
            R"pb(
              wrapper: PRIMITIVE_TYPE_UNSPECIFIED
            )pb",
            absl::InvalidArgumentError("")},
        TestCase{
            R"pb(
              function {
                result_type { primitive: BOOL }
                arg_types { primitive: INT64 }
                arg_types { primitive: STRING }
              })pb",
            absl::InvalidArgumentError("")}));

}  // namespace
}  // namespace cel
