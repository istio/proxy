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

#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;

using TimestampValueTest = common_internal::ValueTest<>;

TEST_F(TimestampValueTest, Kind) {
  EXPECT_EQ(TimestampValue().kind(), TimestampValue::kKind);
  EXPECT_EQ(Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))).kind(),
            TimestampValue::kKind);
}

TEST_F(TimestampValueTest, DebugString) {
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

TEST_F(TimestampValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(TimestampValue().ConvertToJson(descriptor_pool(),
                                             message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(
                            R"pb(string_value: "1970-01-01T00:00:00Z")pb"));
}

TEST_F(TimestampValueTest, NativeTypeId) {
  EXPECT_EQ(
      NativeTypeId::Of(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))),
      NativeTypeId::For<TimestampValue>());
  EXPECT_EQ(NativeTypeId::Of(
                Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
            NativeTypeId::For<TimestampValue>());
}

TEST_F(TimestampValueTest, Equality) {
  EXPECT_NE(TimestampValue(absl::UnixEpoch()),
            absl::UnixEpoch() + absl::Seconds(1));
  EXPECT_NE(absl::UnixEpoch() + absl::Seconds(1),
            TimestampValue(absl::UnixEpoch()));
  EXPECT_NE(TimestampValue(absl::UnixEpoch()),
            TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
}

TEST_F(TimestampValueTest, Comparison) {
  EXPECT_LT(TimestampValue(absl::UnixEpoch()),
            TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
  EXPECT_FALSE(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)) <
               TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
  EXPECT_FALSE(TimestampValue(absl::UnixEpoch() + absl::Seconds(2)) <
               TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
}

}  // namespace
}  // namespace cel
