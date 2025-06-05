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

#include "internal/proto_time_encoding.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/time/time.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace cel::internal {
namespace {

using ::google::api::expr::testutil::EqualsProto;

TEST(EncodeDuration, Basic) {
  google::protobuf::Duration proto_duration;
  ASSERT_OK(
      EncodeDuration(absl::Seconds(2) + absl::Nanoseconds(3), &proto_duration));

  EXPECT_THAT(proto_duration, EqualsProto("seconds: 2 nanos: 3"));
}

TEST(EncodeDurationToString, Basic) {
  ASSERT_OK_AND_ASSIGN(
      std::string json,
      EncodeDurationToString(absl::Seconds(5) + absl::Nanoseconds(20)));
  EXPECT_EQ(json, "5.000000020s");
}

TEST(EncodeTime, Basic) {
  google::protobuf::Timestamp proto_timestamp;
  ASSERT_OK(EncodeTime(absl::FromUnixMillis(300000), &proto_timestamp));

  EXPECT_THAT(proto_timestamp, EqualsProto("seconds: 300"));
}

TEST(EncodeTimeToString, Basic) {
  ASSERT_OK_AND_ASSIGN(std::string json,
                       EncodeTimeToString(absl::FromUnixMillis(80030)));

  EXPECT_EQ(json, "1970-01-01T00:01:20.030Z");
}

TEST(DecodeDuration, Basic) {
  google::protobuf::Duration proto_duration;
  proto_duration.set_seconds(450);
  proto_duration.set_nanos(4);

  EXPECT_EQ(DecodeDuration(proto_duration),
            absl::Seconds(450) + absl::Nanoseconds(4));
}

TEST(DecodeTime, Basic) {
  google::protobuf::Timestamp proto_timestamp;
  proto_timestamp.set_seconds(450);

  EXPECT_EQ(DecodeTime(proto_timestamp), absl::FromUnixSeconds(450));
}

}  // namespace
}  // namespace cel::internal
