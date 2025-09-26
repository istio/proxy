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

#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::testing::IsEmpty;

using DurationValueTest = common_internal::ValueTest<>;

TEST_F(DurationValueTest, Kind) {
  EXPECT_EQ(DurationValue().kind(), DurationValue::kKind);
  EXPECT_EQ(Value(DurationValue(absl::Seconds(1))).kind(),
            DurationValue::kKind);
}

TEST_F(DurationValueTest, DebugString) {
  {
    std::ostringstream out;
    out << DurationValue(absl::Seconds(1));
    EXPECT_EQ(out.str(), "1s");
  }
  {
    std::ostringstream out;
    out << Value(DurationValue(absl::Seconds(1)));
    EXPECT_EQ(out.str(), "1s");
  }
}

TEST_F(DurationValueTest, SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(DurationValue().SerializeTo(descriptor_pool(), message_factory(),
                                          &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), IsEmpty());
}

TEST_F(DurationValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(DurationValue().ConvertToJson(descriptor_pool(),
                                            message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(string_value: "0s")pb"));
}

TEST_F(DurationValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationValue(absl::Seconds(1))),
            NativeTypeId::For<DurationValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(DurationValue(absl::Seconds(1)))),
            NativeTypeId::For<DurationValue>());
}

TEST_F(DurationValueTest, Equality) {
  EXPECT_NE(DurationValue(absl::ZeroDuration()), absl::Seconds(1));
  EXPECT_NE(absl::Seconds(1), DurationValue(absl::ZeroDuration()));
  EXPECT_NE(DurationValue(absl::ZeroDuration()),
            DurationValue(absl::Seconds(1)));
}

TEST_F(DurationValueTest, Comparison) {
  EXPECT_LT(DurationValue(absl::ZeroDuration()), absl::Seconds(1));
  EXPECT_FALSE(DurationValue(absl::Seconds(1)) <
               DurationValue(absl::Seconds(1)));
  EXPECT_FALSE(DurationValue(absl::Seconds(2)) <
               DurationValue(absl::Seconds(1)));
}

}  // namespace
}  // namespace cel
