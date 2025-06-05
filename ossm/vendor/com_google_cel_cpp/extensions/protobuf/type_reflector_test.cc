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

#include "extensions/protobuf/type_reflector.h"

#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"

namespace cel::extensions {
namespace {

using ::absl_testing::StatusIs;
using ::google::api::expr::test::v1::proto2::TestAllTypes;
using ::testing::IsNull;
using ::testing::NotNull;

class ProtoTypeReflectorTest
    : public common_internal::ThreadCompatibleValueTest<> {
 private:
  Shared<TypeReflector> NewTypeReflector(
      MemoryManagerRef memory_manager) override {
    return memory_manager.MakeShared<ProtoTypeReflector>();
  }
};

TEST_P(ProtoTypeReflectorTest, NewStructValueBuilder_NoSuchType) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      value_manager().NewStructValueBuilder(
          common_internal::MakeBasicStructType("message.that.does.not.Exist")));
  EXPECT_THAT(builder, IsNull());
}

TEST_P(ProtoTypeReflectorTest, NewStructValueBuilder_SetFieldByNumber) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewStructValueBuilder(
                           MessageType(TestAllTypes::descriptor())));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByNumber(13, UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoTypeReflectorTest, NewStructValueBuilder_TypeConversionError) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewStructValueBuilder(
                           MessageType(TestAllTypes::descriptor())));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("single_bool", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_int32", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_int64", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_uint32", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_uint64", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_float", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_double", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_string", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_bytes", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_bool_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_int32_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_int64_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_uint32_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_uint64_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_float_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_double_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_string_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("single_bytes_wrapper", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("null_value", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("repeated_bool", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("map_bool_bool", UnknownValue{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(
    ProtoTypeReflectorTest, ProtoTypeReflectorTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ProtoTypeReflectorTest::ToString);

}  // namespace
}  // namespace cel::extensions
