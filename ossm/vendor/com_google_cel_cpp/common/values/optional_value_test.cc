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

#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::DurationValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::StringValueIs;
using ::cel::test::TimestampValueIs;
using ::cel::test::UintValueIs;

class OptionalValueTest : public common_internal::ValueTest<> {
 public:
  OptionalValue OptionalNone() { return OptionalValue::None(); }

  OptionalValue OptionalOf(Value value) {
    return OptionalValue::Of(std::move(value), arena());
  }
};

TEST_F(OptionalValueTest, Kind) {
  EXPECT_EQ(OptionalValue::kind(), OptionalValue::kKind);
}

TEST_F(OptionalValueTest, GetRuntimeType) {
  EXPECT_EQ(OptionalValue().GetRuntimeType(), OptionalType());
  EXPECT_EQ(OpaqueValue(OptionalValue()).GetRuntimeType(), OptionalType());
}

TEST_F(OptionalValueTest, DebugString) {
  EXPECT_EQ(OptionalValue().DebugString(), "optional.none()");
  EXPECT_EQ(OptionalOf(NullValue()).DebugString(), "optional.of(null)");
  EXPECT_EQ(OptionalOf(TrueValue()).DebugString(), "optional.of(true)");
  EXPECT_EQ(OptionalOf(IntValue(1)).DebugString(), "optional.of(1)");
  EXPECT_EQ(OptionalOf(UintValue(1u)).DebugString(), "optional.of(1u)");
  EXPECT_EQ(OptionalOf(DoubleValue(1.0)).DebugString(), "optional.of(1.0)");
  EXPECT_EQ(OptionalOf(DurationValue()).DebugString(), "optional.of(0)");
  EXPECT_EQ(OptionalOf(TimestampValue()).DebugString(),
            "optional.of(1970-01-01T00:00:00Z)");
  EXPECT_EQ(OptionalOf(StringValue()).DebugString(), "optional.of(\"\")");
}

TEST_F(OptionalValueTest, SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(OptionalValue().SerializeTo(descriptor_pool(), message_factory(),
                                          &output),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(OpaqueValue(OptionalValue())
                  .SerializeTo(descriptor_pool(), message_factory(), &output),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(OptionalValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(OptionalValue().ConvertToJson(descriptor_pool(),
                                            message_factory(), message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(OpaqueValue(OptionalValue())
                  .ConvertToJson(descriptor_pool(), message_factory(), message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(OptionalValueTest, GetTypeId) {
  EXPECT_EQ(OpaqueValue(OptionalValue()).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(NullValue())).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(TrueValue())).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(IntValue(1))).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(UintValue(1u))).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(DoubleValue(1.0))).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(DurationValue())).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(TimestampValue())).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
  EXPECT_EQ(OpaqueValue(OptionalOf(StringValue())).GetTypeId(),
            NativeTypeId::For<OptionalValue>());
}

TEST_F(OptionalValueTest, HasValue) {
  EXPECT_FALSE(OptionalValue().HasValue());
  EXPECT_TRUE(OptionalOf(NullValue()).HasValue());
  EXPECT_TRUE(OptionalOf(TrueValue()).HasValue());
  EXPECT_TRUE(OptionalOf(IntValue(1)).HasValue());
  EXPECT_TRUE(OptionalOf(UintValue(1u)).HasValue());
  EXPECT_TRUE(OptionalOf(DoubleValue(1.0)).HasValue());
  EXPECT_TRUE(OptionalOf(DurationValue()).HasValue());
  EXPECT_TRUE(OptionalOf(TimestampValue()).HasValue());
  EXPECT_TRUE(OptionalOf(StringValue()).HasValue());
}

TEST_F(OptionalValueTest, Value) {
  EXPECT_THAT(OptionalValue().Value(),
              ErrorValueIs(StatusIs(absl::StatusCode::kFailedPrecondition)));
  EXPECT_THAT(OptionalOf(NullValue()).Value(), IsNullValue());
  EXPECT_THAT(OptionalOf(TrueValue()).Value(), BoolValueIs(true));
  EXPECT_THAT(OptionalOf(IntValue(1)).Value(), IntValueIs(1));
  EXPECT_THAT(OptionalOf(UintValue(1u)).Value(), UintValueIs(1u));
  EXPECT_THAT(OptionalOf(DoubleValue(1.0)).Value(), DoubleValueIs(1.0));
  EXPECT_THAT(OptionalOf(DurationValue()).Value(),
              DurationValueIs(absl::ZeroDuration()));
  EXPECT_THAT(OptionalOf(TimestampValue()).Value(),
              TimestampValueIs(absl::UnixEpoch()));
  EXPECT_THAT(OptionalOf(StringValue()).Value(), StringValueIs(""));
}

}  // namespace
}  // namespace cel
