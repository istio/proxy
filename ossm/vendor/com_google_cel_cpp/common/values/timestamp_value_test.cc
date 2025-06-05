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

#include "absl/strings/cord.h"
#include "absl/time/time.h"
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

using TimestampValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(TimestampValueTest, Kind) {
  EXPECT_EQ(TimestampValue().kind(), TimestampValue::kKind);
  EXPECT_EQ(Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))).kind(),
            TimestampValue::kKind);
}

TEST_P(TimestampValueTest, DebugString) {
  {
    std::ostringstream out;
    out << TimestampValue(absl::UnixEpoch() + absl::Seconds(1));
    EXPECT_EQ(out.str(), "1970-01-01T00:00:01Z");
  }
  {
    std::ostringstream out;
    out << Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
    EXPECT_EQ(out.str(), "1970-01-01T00:00:01Z");
  }
}

TEST_P(TimestampValueTest, ConvertToJson) {
  EXPECT_THAT(TimestampValue().ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonString("1970-01-01T00:00:00Z"))));
}

TEST_P(TimestampValueTest, NativeTypeId) {
  EXPECT_EQ(
      NativeTypeId::Of(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))),
      NativeTypeId::For<TimestampValue>());
  EXPECT_EQ(NativeTypeId::Of(
                Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
            NativeTypeId::For<TimestampValue>());
}

TEST_P(TimestampValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TimestampValue>(
      TimestampValue(absl::UnixEpoch() + absl::Seconds(1))));
  EXPECT_TRUE(InstanceOf<TimestampValue>(
      Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))));
}

TEST_P(TimestampValueTest, Cast) {
  EXPECT_THAT(Cast<TimestampValue>(
                  TimestampValue(absl::UnixEpoch() + absl::Seconds(1))),
              An<TimestampValue>());
  EXPECT_THAT(Cast<TimestampValue>(
                  Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
              An<TimestampValue>());
}

TEST_P(TimestampValueTest, As) {
  EXPECT_THAT(As<TimestampValue>(
                  Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
              Ne(absl::nullopt));
}

TEST_P(TimestampValueTest, Equality) {
  EXPECT_NE(TimestampValue(absl::UnixEpoch()),
            absl::UnixEpoch() + absl::Seconds(1));
  EXPECT_NE(absl::UnixEpoch() + absl::Seconds(1),
            TimestampValue(absl::UnixEpoch()));
  EXPECT_NE(TimestampValue(absl::UnixEpoch()),
            TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
}

INSTANTIATE_TEST_SUITE_P(
    TimestampValueTest, TimestampValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    TimestampValueTest::ToString);

}  // namespace
}  // namespace cel
