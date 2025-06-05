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
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::An;
using ::testing::Ne;

using UnknownValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(UnknownValueTest, Kind) {
  EXPECT_EQ(UnknownValue().kind(), UnknownValue::kKind);
  EXPECT_EQ(Value(UnknownValue()).kind(), UnknownValue::kKind);
}

TEST_P(UnknownValueTest, DebugString) {
  {
    std::ostringstream out;
    out << UnknownValue();
    EXPECT_EQ(out.str(), "");
  }
  {
    std::ostringstream out;
    out << Value(UnknownValue());
    EXPECT_EQ(out.str(), "");
  }
}

TEST_P(UnknownValueTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(UnknownValue().SerializeTo(value_manager(), value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(UnknownValueTest, ConvertToJson) {
  EXPECT_THAT(UnknownValue().ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(UnknownValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UnknownValue()),
            NativeTypeId::For<UnknownValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(UnknownValue())),
            NativeTypeId::For<UnknownValue>());
}

TEST_P(UnknownValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UnknownValue>(UnknownValue()));
  EXPECT_TRUE(InstanceOf<UnknownValue>(Value(UnknownValue())));
}

TEST_P(UnknownValueTest, Cast) {
  EXPECT_THAT(Cast<UnknownValue>(UnknownValue()), An<UnknownValue>());
  EXPECT_THAT(Cast<UnknownValue>(Value(UnknownValue())), An<UnknownValue>());
}

TEST_P(UnknownValueTest, As) {
  EXPECT_THAT(As<UnknownValue>(Value(UnknownValue())), Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    UnknownValueTest, UnknownValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    UnknownValueTest::ToString);

}  // namespace
}  // namespace cel
