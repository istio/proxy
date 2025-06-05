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
#include <utility>

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::An;
using ::testing::Ne;
using ::testing::TestParamInfo;

class OptionalValueTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  OptionalValue OptionalNone() { return OptionalValue::None(); }

  OptionalValue OptionalOf(Value value) {
    return OptionalValue::Of(memory_manager(), std::move(value));
  }
};

TEST_P(OptionalValueTest, Kind) {
  auto value = OptionalNone();
  EXPECT_EQ(value.kind(), OptionalValue::kKind);
  EXPECT_EQ(OpaqueValue(value).kind(), OptionalValue::kKind);
  EXPECT_EQ(Value(value).kind(), OptionalValue::kKind);
}

TEST_P(OptionalValueTest, Type) {
  auto value = OptionalNone();
  EXPECT_EQ(value.GetRuntimeType(), OptionalType());
}

TEST_P(OptionalValueTest, DebugString) {
  auto value = OptionalNone();
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OpaqueValue(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OptionalOf(IntValue());
    EXPECT_EQ(out.str(), "optional(0)");
  }
}

TEST_P(OptionalValueTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(OptionalValue().SerializeTo(value_manager(), value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, ConvertToJson) {
  EXPECT_THAT(OptionalValue().ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, InstanceOf) {
  auto value = OptionalNone();
  EXPECT_TRUE(InstanceOf<OptionalValue>(value));
  EXPECT_TRUE(InstanceOf<OptionalValue>(OpaqueValue(value)));
  EXPECT_TRUE(InstanceOf<OptionalValue>(Value(value)));
}

TEST_P(OptionalValueTest, Cast) {
  auto value = OptionalNone();
  EXPECT_THAT(Cast<OptionalValue>(value), An<OptionalValue>());
  EXPECT_THAT(Cast<OptionalValue>(OpaqueValue(value)), An<OptionalValue>());
  EXPECT_THAT(Cast<OptionalValue>(Value(value)), An<OptionalValue>());
}

TEST_P(OptionalValueTest, As) {
  auto value = OptionalNone();
  EXPECT_THAT(As<OptionalValue>(OpaqueValue(value)), Ne(absl::nullopt));
  EXPECT_THAT(As<OptionalValue>(Value(value)), Ne(absl::nullopt));
}

TEST_P(OptionalValueTest, HasValue) {
  auto value = OptionalNone();
  EXPECT_FALSE(value.HasValue());
  value = OptionalOf(IntValue());
  EXPECT_TRUE(value.HasValue());
}

TEST_P(OptionalValueTest, Value) {
  auto value = OptionalNone();
  auto element = value.Value();
  ASSERT_TRUE(InstanceOf<ErrorValue>(element));
  EXPECT_THAT(Cast<ErrorValue>(element).NativeValue(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  value = OptionalOf(IntValue());
  element = value.Value();
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  EXPECT_EQ(Cast<IntValue>(element), IntValue());
}

INSTANTIATE_TEST_SUITE_P(
    OptionalValueTest, OptionalValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OptionalValueTest::ToString);

}  // namespace
}  // namespace cel
