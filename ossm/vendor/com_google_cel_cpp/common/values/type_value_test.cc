// Copyright 2023 Google LLC
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

#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::An;
using ::testing::Ne;

using TypeValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(TypeValueTest, Kind) {
  EXPECT_EQ(TypeValue(AnyType()).kind(), TypeValue::kKind);
  EXPECT_EQ(Value(TypeValue(AnyType())).kind(), TypeValue::kKind);
}

TEST_P(TypeValueTest, DebugString) {
  {
    std::ostringstream out;
    out << TypeValue(AnyType());
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
  {
    std::ostringstream out;
    out << Value(TypeValue(AnyType()));
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
}

TEST_P(TypeValueTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(TypeValue(AnyType()).SerializeTo(value_manager(), value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeValueTest, ConvertToJson) {
  EXPECT_THAT(TypeValue(AnyType()).ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeValue(AnyType())),
            NativeTypeId::For<TypeValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(TypeValue(AnyType()))),
            NativeTypeId::For<TypeValue>());
}

TEST_P(TypeValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TypeValue>(TypeValue(AnyType())));
  EXPECT_TRUE(InstanceOf<TypeValue>(Value(TypeValue(AnyType()))));
}

TEST_P(TypeValueTest, Cast) {
  EXPECT_THAT(Cast<TypeValue>(TypeValue(AnyType())), An<TypeValue>());
  EXPECT_THAT(Cast<TypeValue>(Value(TypeValue(AnyType()))), An<TypeValue>());
}

TEST_P(TypeValueTest, As) {
  EXPECT_THAT(As<TypeValue>(Value(TypeValue(AnyType()))), Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    TypeValueTest, TypeValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    TypeValueTest::ToString);

}  // namespace
}  // namespace cel
