// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_COMPAT_PROTO_UTILS_H_
#define OCPDIAG_CORE_COMPAT_PROTO_UTILS_H_

#include "google/protobuf/duration.pb.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace ocpdiag {

// Decodes the given duration protobuf and returns an absl::Duration, or returns
// an error status if the argument is invalid according to
//
absl::StatusOr<absl::Duration> DecodeDurationProto(
    const google::protobuf::Duration& proto);

}  // namespace ocpdiag

#endif  // OCPDIAG_CORE_COMPAT_PROTO_UTILS_H_
