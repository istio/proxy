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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_PROVIDER_H_

#include "absl/status/statusor.h"
#include "base/function_descriptor.h"
#include "runtime/activation_interface.h"
#include "runtime/function_overload_reference.h"

namespace cel::runtime_internal {

// Interface for providers of lazily bound functions.
//
// Lazily bound functions may have an implementation that is dependent on the
// evaluation context (as represented by the Activation).
class FunctionProvider {
 public:
  virtual ~FunctionProvider() = default;

  // Returns a reference to a function implementation based on the provided
  // Activation. Given the same activation, this should return the same Function
  // instance. The cel::FunctionOverloadReference is assumed to be stable for
  // the life of the Activation.
  //
  // An empty optional result is interpreted as no matching overload.
  virtual absl::StatusOr<absl::optional<FunctionOverloadReference>> GetFunction(
      const FunctionDescriptor& descriptor,
      const ActivationInterface& activation) const = 0;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_PROVIDER_H_
