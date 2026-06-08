// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/compat/proto_utils.h"

#include "google/protobuf/duration.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ocpdiag/core/compat/status_macros.h"

namespace ocpdiag {

// Validation requirements documented at:
//
absl::Status Validate(const google::protobuf::Duration& d) {
  const auto sec = d.seconds();
  const auto ns = d.nanos();
  if (sec < -315576000000 || sec > 315576000000) {
    return absl::InvalidArgumentError(absl::StrCat("seconds=", sec));
  }
  if (ns < -999999999 || ns > 999999999) {
    return absl::InvalidArgumentError(absl::StrCat("nanos=", ns));
  }
  if ((sec < 0 && ns > 0) || (sec > 0 && ns < 0)) {
    return absl::InvalidArgumentError("sign mismatch");
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Duration> DecodeDurationProto(
    const google::protobuf::Duration& proto) {
  RETURN_IF_ERROR(Validate(proto));
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

}  // namespace ocpdiag
