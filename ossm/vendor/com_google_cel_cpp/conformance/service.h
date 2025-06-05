// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_CONFORMANCE_SERVICE_H_
#define THIRD_PARTY_CEL_CPP_CONFORMANCE_SERVICE_H_

#include <memory>

#include "google/api/expr/conformance/v1alpha1/conformance_service.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace cel_conformance {

class ConformanceServiceInterface {
 public:
  virtual ~ConformanceServiceInterface() = default;

  virtual void Parse(
      const google::api::expr::conformance::v1alpha1::ParseRequest& request,
      google::api::expr::conformance::v1alpha1::ParseResponse& response) = 0;

  virtual void Check(
      const google::api::expr::conformance::v1alpha1::CheckRequest& request,
      google::api::expr::conformance::v1alpha1::CheckResponse& response) = 0;

  virtual absl::Status Eval(
      const google::api::expr::conformance::v1alpha1::EvalRequest& request,
      google::api::expr::conformance::v1alpha1::EvalResponse& response) = 0;
};

struct ConformanceServiceOptions {
  bool optimize;
  bool modern;
  bool arena;
  bool recursive;
};

absl::StatusOr<std::unique_ptr<ConformanceServiceInterface>>
NewConformanceService(const ConformanceServiceOptions&);

}  // namespace cel_conformance

#endif  // THIRD_PARTY_CEL_CPP_CONFORMANCE_SERVICE_H_
