// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_COMPAT_GRPC_STATUS_CONVERTERS_H_
#define OCPDIAG_CORE_COMPAT_GRPC_STATUS_CONVERTERS_H_

#include "grpcpp/support/status.h"
#include "absl/status/status.h"

namespace ocpdiag {

// Compatibility method for differences between internal/external
// gRPC and abseil status libraries. In google, the two statuses can
// be implicitly cast to each other, externally that causes build breakage.

inline grpc::Status ToGRPCStatus(const absl::Status& status) {
  return grpc::Status(
  static_cast<grpc::StatusCode>(status.code()),
  std::string(status.message()));
}

}  // namespace ocpdiag
#endif  // OCPDIAG_CORE_COMPAT_GRPC_STATUS_CONVERTERS_H_
