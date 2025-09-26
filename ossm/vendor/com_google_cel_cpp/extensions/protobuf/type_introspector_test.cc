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

#include "extensions/protobuf/type_introspector.h"

#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto2/test_all_types.pb.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::cel::expr::conformance::proto2::TestAllTypes;
using ::testing::Eq;
using ::testing::Optional;

TEST(ProtoTypeIntrospector, FindType) {
  ProtoTypeIntrospector introspector;
  EXPECT_THAT(
      introspector.FindType(TestAllTypes::descriptor()->full_name()),
      IsOkAndHolds(Optional(Eq(MessageType(TestAllTypes::GetDescriptor())))));
  EXPECT_THAT(introspector.FindType("type.that.does.not.Exist"),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST(ProtoTypeIntrospector, FindStructTypeFieldByName) {
  ProtoTypeIntrospector introspector;
  ASSERT_OK_AND_ASSIGN(
      auto field, introspector.FindStructTypeFieldByName(
                      TestAllTypes::descriptor()->full_name(), "single_int32"));
  ASSERT_TRUE(field.has_value());
  EXPECT_THAT(field->name(), Eq("single_int32"));
  EXPECT_THAT(field->number(), Eq(1));
  EXPECT_THAT(
      introspector.FindStructTypeFieldByName(
          TestAllTypes::descriptor()->full_name(), "field_that_does_not_exist"),
      IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(introspector.FindStructTypeFieldByName("type.that.does.not.Exist",
                                                     "does_not_matter"),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST(ProtoTypeIntrospector, FindEnumConstant) {
  ProtoTypeIntrospector introspector;
  const auto* enum_desc = TestAllTypes::NestedEnum_descriptor();
  ASSERT_OK_AND_ASSIGN(
      auto enum_constant,
      introspector.FindEnumConstant(
          "cel.expr.conformance.proto2.TestAllTypes.NestedEnum", "BAZ"));
  ASSERT_TRUE(enum_constant.has_value());
  EXPECT_EQ(enum_constant->type.kind(), TypeKind::kEnum);
  EXPECT_EQ(enum_constant->type_full_name, enum_desc->full_name());
  EXPECT_EQ(enum_constant->value_name, "BAZ");
  EXPECT_EQ(enum_constant->number, 2);
}

TEST(ProtoTypeIntrospector, FindEnumConstantNull) {
  ProtoTypeIntrospector introspector;
  ASSERT_OK_AND_ASSIGN(
      auto enum_constant,
      introspector.FindEnumConstant("google.protobuf.NullValue", "NULL_VALUE"));
  ASSERT_TRUE(enum_constant.has_value());
  EXPECT_EQ(enum_constant->type.kind(), TypeKind::kNull);
  EXPECT_EQ(enum_constant->type_full_name, "google.protobuf.NullValue");
  EXPECT_EQ(enum_constant->value_name, "NULL_VALUE");
  EXPECT_EQ(enum_constant->number, 0);
}

TEST(ProtoTypeIntrospector, FindEnumConstantUnknownEnum) {
  ProtoTypeIntrospector introspector;

  ASSERT_OK_AND_ASSIGN(auto enum_constant,
                       introspector.FindEnumConstant("NotARealEnum", "BAZ"));
  EXPECT_FALSE(enum_constant.has_value());
}

TEST(ProtoTypeIntrospector, FindEnumConstantUnknownValue) {
  ProtoTypeIntrospector introspector;

  ASSERT_OK_AND_ASSIGN(
      auto enum_constant,
      introspector.FindEnumConstant(
          "cel.expr.conformance.proto2.TestAllTypes.NestedEnum", "QUX"));
  ASSERT_FALSE(enum_constant.has_value());
}

}  // namespace
}  // namespace cel::extensions
