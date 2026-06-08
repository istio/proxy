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

#include "runtime/internal/function_adapter.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "common/kind.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel::runtime_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;

static_assert(AdaptedKind<int64_t>() == Kind::kInt, "int adapts to int64_t");
static_assert(AdaptedKind<uint64_t>() == Kind::kUint,
              "uint adapts to uint64_t");
static_assert(AdaptedKind<double>() == Kind::kDouble,
              "double adapts to double");
static_assert(AdaptedKind<bool>() == Kind::kBool, "bool adapts to bool");
static_assert(AdaptedKind<absl::Time>() == Kind::kTimestamp,
              "timestamp adapts to absl::Time");
static_assert(AdaptedKind<absl::Duration>() == Kind::kDuration,
              "duration adapts to absl::Duration");
// Handle types.
static_assert(AdaptedKind<Value>() == Kind::kAny, "any adapts to Value");
static_assert(AdaptedKind<StringValue>() == Kind::kString,
              "string adapts to String");
static_assert(AdaptedKind<BytesValue>() == Kind::kBytes,
              "bytes adapts to Bytes");
static_assert(AdaptedKind<StructValue>() == Kind::kStruct,
              "struct adapts to StructValue");
static_assert(AdaptedKind<ListValue>() == Kind::kList,
              "list adapts to ListValue");
static_assert(AdaptedKind<MapValue>() == Kind::kMap, "map adapts to MapValue");
static_assert(AdaptedKind<NullValue>() == Kind::kNullType,
              "null adapts to NullValue");
static_assert(AdaptedKind<const Value&>() == Kind::kAny,
              "any adapts to const Value&");
static_assert(AdaptedKind<const StringValue&>() == Kind::kString,
              "string adapts to const String&");
static_assert(AdaptedKind<const BytesValue&>() == Kind::kBytes,
              "bytes adapts to const Bytes&");
static_assert(AdaptedKind<const StructValue&>() == Kind::kStruct,
              "struct adapts to const StructValue&");
static_assert(AdaptedKind<const ListValue&>() == Kind::kList,
              "list adapts to const ListValue&");
static_assert(AdaptedKind<const MapValue&>() == Kind::kMap,
              "map adapts to const MapValue&");
static_assert(AdaptedKind<const NullValue&>() == Kind::kNullType,
              "null adapts to const NullValue&");

class ValueToAdaptedVisitorTest : public ::testing::Test {};

TEST_F(ValueToAdaptedVisitorTest, Int) {
  Value v = cel::IntValue(10);

  int64_t out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out, 10);
}

TEST_F(ValueToAdaptedVisitorTest, IntWrongKind) {
  Value v = cel::UintValue(10);

  int64_t out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected int value"));
}

TEST_F(ValueToAdaptedVisitorTest, Uint) {
  Value v = cel::UintValue(11);

  uint64_t out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out, 11);
}

TEST_F(ValueToAdaptedVisitorTest, UintWrongKind) {
  Value v = cel::IntValue(11);

  uint64_t out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected uint value"));
}

TEST_F(ValueToAdaptedVisitorTest, Double) {
  Value v = cel::DoubleValue(12.0);

  double out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out, 12.0);
}

TEST_F(ValueToAdaptedVisitorTest, DoubleWrongKind) {
  Value v = cel::UintValue(10);

  double out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected double value"));
}

TEST_F(ValueToAdaptedVisitorTest, Bool) {
  Value v = cel::BoolValue(false);

  bool out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out, false);
}

TEST_F(ValueToAdaptedVisitorTest, BoolWrongKind) {
  Value v = cel::UintValue(10);

  bool out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected bool value"));
}

TEST_F(ValueToAdaptedVisitorTest, Timestamp) {
  Value v = cel::TimestampValue(absl::UnixEpoch() + absl::Seconds(1));

  absl::Time out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out, absl::UnixEpoch() + absl::Seconds(1));
}

TEST_F(ValueToAdaptedVisitorTest, TimestampWrongKind) {
  Value v = cel::UintValue(10);

  absl::Time out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected timestamp value"));
}

TEST_F(ValueToAdaptedVisitorTest, Duration) {
  Value v = cel::DurationValue(absl::Seconds(5));

  absl::Duration out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out, absl::Seconds(5));
}

TEST_F(ValueToAdaptedVisitorTest, DurationWrongKind) {
  Value v = cel::UintValue(10);

  absl::Duration out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected duration value"));
}

TEST_F(ValueToAdaptedVisitorTest, String) {
  Value v = cel::StringValue("string");

  StringValue out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out.ToString(), "string");
}

TEST_F(ValueToAdaptedVisitorTest, StringWrongKind) {
  Value v = cel::UintValue(10);

  StringValue out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected string value"));
}

TEST_F(ValueToAdaptedVisitorTest, Bytes) {
  Value v = cel::BytesValue("bytes");

  BytesValue out;
  ASSERT_THAT(ValueToAdaptedVisitor{v}(&out), IsOk());

  EXPECT_EQ(out.ToString(), "bytes");
}

TEST_F(ValueToAdaptedVisitorTest, BytesWrongKind) {
  Value v = cel::UintValue(10);

  BytesValue out;
  EXPECT_THAT(
      ValueToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected bytes value"));
}

class AdaptedToValueVisitorTest : public ::testing::Test {};

TEST_F(AdaptedToValueVisitorTest, Int) {
  int64_t value = 10;

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsInt());
  EXPECT_EQ(result.GetInt().NativeValue(), 10);
}

TEST_F(AdaptedToValueVisitorTest, Double) {
  double value = 10;

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsDouble());
  EXPECT_EQ(result.GetDouble().NativeValue(), 10.0);
}

TEST_F(AdaptedToValueVisitorTest, Uint) {
  uint64_t value = 10;

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsUint());
  EXPECT_EQ(result.GetUint().NativeValue(), 10);
}

TEST_F(AdaptedToValueVisitorTest, Bool) {
  bool value = true;

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.GetBool().NativeValue(), true);
}

TEST_F(AdaptedToValueVisitorTest, Timestamp) {
  absl::Time value = absl::UnixEpoch() + absl::Seconds(10);

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsTimestamp());
  EXPECT_EQ(result.GetTimestamp().ToTime(),
            absl::UnixEpoch() + absl::Seconds(10));
}

TEST_F(AdaptedToValueVisitorTest, Duration) {
  absl::Duration value = absl::Seconds(5);

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsDuration());
  EXPECT_EQ(result.GetDuration().ToDuration(), absl::Seconds(5));
}

TEST_F(AdaptedToValueVisitorTest, String) {
  StringValue value = cel::StringValue("str");

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.GetString().ToString(), "str");
}

TEST_F(AdaptedToValueVisitorTest, Bytes) {
  BytesValue value = cel::BytesValue("bytes");

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsBytes());
  EXPECT_EQ(result.GetBytes().ToString(), "bytes");
}

TEST_F(AdaptedToValueVisitorTest, StatusOrValue) {
  absl::StatusOr<int64_t> value = 10;

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(value));

  ASSERT_TRUE(result.IsInt());
  EXPECT_EQ(result.GetInt().NativeValue(), 10);
}

TEST_F(AdaptedToValueVisitorTest, StatusOrError) {
  absl::StatusOr<int64_t> value = absl::InternalError("test_error");

  EXPECT_THAT(AdaptedToValueVisitor{}(value).status(),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(AdaptedToValueVisitorTest, Any) {
  auto handle = cel::ErrorValue(absl::InternalError("test_error"));

  ASSERT_OK_AND_ASSIGN(auto result, AdaptedToValueVisitor{}(handle));

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.GetError().NativeValue(),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

}  // namespace
}  // namespace cel::runtime_internal
