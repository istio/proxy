// Copyright 2021 Google LLC
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

#include "internal/time.h"

#include <string>

#include "google/protobuf/util/time_util.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

using ::absl_testing::StatusIs;

TEST(MaxDuration, ProtoEquiv) {
  EXPECT_EQ(MaxDuration(),
            absl::Seconds(google::protobuf::util::TimeUtil::kDurationMaxSeconds) +
                absl::Nanoseconds(999999999));
}

TEST(MinDuration, ProtoEquiv) {
  EXPECT_EQ(MinDuration(),
            absl::Seconds(google::protobuf::util::TimeUtil::kDurationMinSeconds) +
                absl::Nanoseconds(-999999999));
}

TEST(MaxTimestamp, ProtoEquiv) {
  EXPECT_EQ(MaxTimestamp(),
            absl::UnixEpoch() +
                absl::Seconds(google::protobuf::util::TimeUtil::kTimestampMaxSeconds) +
                absl::Nanoseconds(999999999));
}

TEST(MinTimestamp, ProtoEquiv) {
  EXPECT_EQ(MinTimestamp(),
            absl::UnixEpoch() +
                absl::Seconds(google::protobuf::util::TimeUtil::kTimestampMinSeconds));
}

TEST(ParseDuration, Conformance) {
  absl::Duration parsed;
  ASSERT_OK_AND_ASSIGN(parsed, internal::ParseDuration("1s"));
  EXPECT_EQ(parsed, absl::Seconds(1));
  ASSERT_OK_AND_ASSIGN(parsed, internal::ParseDuration("0.010s"));
  EXPECT_EQ(parsed, absl::Milliseconds(10));
  ASSERT_OK_AND_ASSIGN(parsed, internal::ParseDuration("0.000010s"));
  EXPECT_EQ(parsed, absl::Microseconds(10));
  ASSERT_OK_AND_ASSIGN(parsed, internal::ParseDuration("0.000000010s"));
  EXPECT_EQ(parsed, absl::Nanoseconds(10));

  EXPECT_THAT(internal::ParseDuration("abc"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(internal::ParseDuration("1c"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(FormatDuration, Conformance) {
  std::string formatted;
  ASSERT_OK_AND_ASSIGN(formatted, internal::FormatDuration(absl::Seconds(1)));
  EXPECT_EQ(formatted, "1s");
  ASSERT_OK_AND_ASSIGN(formatted,
                       internal::FormatDuration(absl::Milliseconds(10)));
  EXPECT_EQ(formatted, "10ms");
  ASSERT_OK_AND_ASSIGN(formatted,
                       internal::FormatDuration(absl::Microseconds(10)));
  EXPECT_EQ(formatted, "10us");
  ASSERT_OK_AND_ASSIGN(formatted,
                       internal::FormatDuration(absl::Nanoseconds(10)));
  EXPECT_EQ(formatted, "10ns");

  EXPECT_THAT(internal::FormatDuration(absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(internal::FormatDuration(-absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ParseTimestamp, Conformance) {
  absl::Time parsed;
  ASSERT_OK_AND_ASSIGN(parsed, internal::ParseTimestamp("1-01-01T00:00:00Z"));
  EXPECT_EQ(parsed, MinTimestamp());
  ASSERT_OK_AND_ASSIGN(
      parsed, internal::ParseTimestamp("9999-12-31T23:59:59.999999999Z"));
  EXPECT_EQ(parsed, MaxTimestamp());
  ASSERT_OK_AND_ASSIGN(parsed,
                       internal::ParseTimestamp("1970-01-01T00:00:00Z"));
  EXPECT_EQ(parsed, absl::UnixEpoch());
  ASSERT_OK_AND_ASSIGN(parsed,
                       internal::ParseTimestamp("1970-01-01T00:00:00.010Z"));
  EXPECT_EQ(parsed, absl::UnixEpoch() + absl::Milliseconds(10));
  ASSERT_OK_AND_ASSIGN(parsed,
                       internal::ParseTimestamp("1970-01-01T00:00:00.000010Z"));
  EXPECT_EQ(parsed, absl::UnixEpoch() + absl::Microseconds(10));
  ASSERT_OK_AND_ASSIGN(
      parsed, internal::ParseTimestamp("1970-01-01T00:00:00.000000010Z"));
  EXPECT_EQ(parsed, absl::UnixEpoch() + absl::Nanoseconds(10));

  EXPECT_THAT(internal::ParseTimestamp("abc"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(internal::ParseTimestamp("10000-01-01T00:00:00Z"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(FormatTimestamp, Conformance) {
  std::string formatted;
  ASSERT_OK_AND_ASSIGN(formatted, internal::FormatTimestamp(MinTimestamp()));
  EXPECT_EQ(formatted, "1-01-01T00:00:00Z");
  ASSERT_OK_AND_ASSIGN(formatted, internal::FormatTimestamp(MaxTimestamp()));
  EXPECT_EQ(formatted, "9999-12-31T23:59:59.999999999Z");
  ASSERT_OK_AND_ASSIGN(formatted, internal::FormatTimestamp(absl::UnixEpoch()));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00Z");
  ASSERT_OK_AND_ASSIGN(
      formatted,
      internal::FormatTimestamp(absl::UnixEpoch() + absl::Milliseconds(10)));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00.01Z");
  ASSERT_OK_AND_ASSIGN(
      formatted,
      internal::FormatTimestamp(absl::UnixEpoch() + absl::Microseconds(10)));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00.00001Z");
  ASSERT_OK_AND_ASSIGN(
      formatted,
      internal::FormatTimestamp(absl::UnixEpoch() + absl::Nanoseconds(10)));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00.00000001Z");

  EXPECT_THAT(internal::FormatTimestamp(absl::InfiniteFuture()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(internal::FormatTimestamp(absl::InfinitePast()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(EncodeDurationToJson, Conformance) {
  std::string formatted;
  ASSERT_OK_AND_ASSIGN(formatted, EncodeDurationToJson(absl::Seconds(1)));
  EXPECT_EQ(formatted, "1s");
  ASSERT_OK_AND_ASSIGN(formatted, EncodeDurationToJson(absl::Milliseconds(10)));
  EXPECT_EQ(formatted, "0.010s");
  ASSERT_OK_AND_ASSIGN(formatted, EncodeDurationToJson(absl::Microseconds(10)));
  EXPECT_EQ(formatted, "0.000010s");
  ASSERT_OK_AND_ASSIGN(formatted, EncodeDurationToJson(absl::Nanoseconds(10)));
  EXPECT_EQ(formatted, "0.000000010s");

  EXPECT_THAT(EncodeDurationToJson(absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(EncodeDurationToJson(-absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(EncodeTimestampToJson, Conformance) {
  std::string formatted;
  ASSERT_OK_AND_ASSIGN(formatted, EncodeTimestampToJson(MinTimestamp()));
  EXPECT_EQ(formatted, "0001-01-01T00:00:00Z");
  ASSERT_OK_AND_ASSIGN(formatted, EncodeTimestampToJson(MaxTimestamp()));
  EXPECT_EQ(formatted, "9999-12-31T23:59:59.999999999Z");
  ASSERT_OK_AND_ASSIGN(formatted, EncodeTimestampToJson(absl::UnixEpoch()));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00Z");
  ASSERT_OK_AND_ASSIGN(
      formatted,
      EncodeTimestampToJson(absl::UnixEpoch() + absl::Milliseconds(10)));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00.010Z");
  ASSERT_OK_AND_ASSIGN(
      formatted,
      EncodeTimestampToJson(absl::UnixEpoch() + absl::Microseconds(10)));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00.000010Z");
  ASSERT_OK_AND_ASSIGN(formatted, EncodeTimestampToJson(absl::UnixEpoch() +
                                                        absl::Nanoseconds(10)));
  EXPECT_EQ(formatted, "1970-01-01T00:00:00.000000010Z");

  EXPECT_THAT(EncodeTimestampToJson(absl::InfiniteFuture()),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(EncodeTimestampToJson(absl::InfinitePast()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace cel::internal
