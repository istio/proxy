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

#include <string>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "google/protobuf/util/time_util.h"

namespace cel::internal {

namespace {

absl::Status Validate(absl::Time time) {
  if (time < cel::internal::MinTimestamp()) {
    return absl::InvalidArgumentError("time below min");
  }

  if (time > cel::internal::MaxTimestamp()) {
    return absl::InvalidArgumentError("time above max");
  }
  return absl::OkStatus();
}

absl::Status CelValidateDuration(absl::Duration duration) {
  if (duration < cel::internal::MinDuration()) {
    return absl::InvalidArgumentError("duration below min");
  }

  if (duration > cel::internal::MaxDuration()) {
    return absl::InvalidArgumentError("duration above max");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Duration DecodeDuration(const google::protobuf::Duration& proto) {
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

absl::Time DecodeTime(const google::protobuf::Timestamp& proto) {
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

absl::Status EncodeDuration(absl::Duration duration,
                            google::protobuf::Duration* proto) {
  CEL_RETURN_IF_ERROR(CelValidateDuration(duration));
  // s and n may both be negative, per the Duration proto spec.
  const int64_t s = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  const int64_t n =
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration);
  proto->set_seconds(s);
  proto->set_nanos(n);
  return absl::OkStatus();
}

absl::StatusOr<std::string> EncodeDurationToString(absl::Duration duration) {
  google::protobuf::Duration d;
  auto status = EncodeDuration(duration, &d);
  if (!status.ok()) {
    return status;
  }
  return google::protobuf::util::TimeUtil::ToString(d);
}

absl::Status EncodeTime(absl::Time time, google::protobuf::Timestamp* proto) {
  CEL_RETURN_IF_ERROR(Validate(time));
  const int64_t s = absl::ToUnixSeconds(time);
  proto->set_seconds(s);
  proto->set_nanos((time - absl::FromUnixSeconds(s)) / absl::Nanoseconds(1));
  return absl::OkStatus();
}

absl::StatusOr<std::string> EncodeTimeToString(absl::Time time) {
  google::protobuf::Timestamp t;
  auto status = EncodeTime(time, &t);
  if (!status.ok()) {
    return status;
  }
  return google::protobuf::util::TimeUtil::ToString(t);
}

}  // namespace cel::internal
