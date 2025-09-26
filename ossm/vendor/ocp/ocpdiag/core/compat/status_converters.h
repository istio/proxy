// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_COMPAT_STATUS_CONVERTERS_H_
#define OCPDIAG_CORE_COMPAT_STATUS_CONVERTERS_H_

#include "grpcpp/support/status.h"
#include "absl/status/status.h"

namespace ocpdiag {

// Compatibility method for differences between internal/external
// gRPC and abseil status libraries. In google, the two statuses can
// be implicitly cast to each other, externally that causes build breakage.

// Conversion helpers between abseil and open-source protobuf status. In google,
// all statuses are abseil so the specialization should always be invoked.
template <typename T>
absl::Status AsAbslStatus(const T& status) {
  static_assert(!std::is_same_v<decltype(status), absl::Status>);
  return absl::Status(absl::StatusCode(static_cast<int>(status.code())),
                      status.message().as_string());
}
// Equivalent for gRPC status.
template <>
inline absl::Status AsAbslStatus(const grpc::Status& status) {
  return absl::Status(absl::StatusCode(static_cast<int>(status.error_code())),
                      status.error_message());
}

}  // namespace ocpdiag
#endif  // OCPDIAG_CORE_COMPAT_STATUS_CONVERTERS_H_
