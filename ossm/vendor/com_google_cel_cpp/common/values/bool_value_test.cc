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

#include "absl/hash/hash.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::An;
using ::testing::Ne;

using BoolValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(BoolValueTest, Kind) {
  EXPECT_EQ(BoolValue(true).kind(), BoolValue::kKind);
  EXPECT_EQ(Value(BoolValue(true)).kind(), BoolValue::kKind);
}

TEST_P(BoolValueTest, DebugString) {
  {
    std::ostringstream out;
    out << BoolValue(true);
    EXPECT_EQ(out.str(), "true");
  }
  {
    std::ostringstream out;
    out << Value(BoolValue(true));
    EXPECT_EQ(out.str(), "true");
  }
}

TEST_P(BoolValueTest, ConvertToJson) {
  EXPECT_THAT(BoolValue(false).ConvertToJson(value_manager()),
              IsOkAndHolds(Json(false)));
}

TEST_P(BoolValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BoolValue(true)), NativeTypeId::For<BoolValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BoolValue(true))),
            NativeTypeId::For<BoolValue>());
}

TEST_P(BoolValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BoolValue>(BoolValue(true)));
  EXPECT_TRUE(InstanceOf<BoolValue>(Value(BoolValue(true))));
}

TEST_P(BoolValueTest, Cast) {
  EXPECT_THAT(Cast<BoolValue>(BoolValue(true)), An<BoolValue>());
  EXPECT_THAT(Cast<BoolValue>(Value(BoolValue(true))), An<BoolValue>());
}

TEST_P(BoolValueTest, As) {
  EXPECT_THAT(As<BoolValue>(Value(BoolValue(true))), Ne(absl::nullopt));
}

TEST_P(BoolValueTest, HashValue) {
  EXPECT_EQ(absl::HashOf(BoolValue(true)), absl::HashOf(true));
}

TEST_P(BoolValueTest, Equality) {
  EXPECT_NE(BoolValue(false), true);
  EXPECT_NE(true, BoolValue(false));
  EXPECT_NE(BoolValue(false), BoolValue(true));
}

TEST_P(BoolValueTest, LessThan) {
  EXPECT_LT(BoolValue(false), true);
  EXPECT_LT(false, BoolValue(true));
  EXPECT_LT(BoolValue(false), BoolValue(true));
}

INSTANTIATE_TEST_SUITE_P(
    BoolValueTest, BoolValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    BoolValueTest::ToString);

}  // namespace
}  // namespace cel
