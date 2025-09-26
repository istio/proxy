// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ACTIVATION_MATCHER_ACCESS_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ACTIVATION_MATCHER_ACCESS_H_

#include <memory>

#include "absl/base/nullability.h"
#include "runtime/internal/attribute_matcher.h"

namespace google::api::expr::runtime {
class Activation;
class BaseActivation;
}  // namespace google::api::expr::runtime

namespace cel {
class Activation;
class ActivationInterface;
}  // namespace cel

namespace cel::runtime_internal {

class ActivationAttributeMatcherAccess {
 public:
  static void SetAttributeMatcher(
      google::api::expr::runtime::Activation& activation,
      const AttributeMatcher* matcher);

  static void SetAttributeMatcher(
      google::api::expr::runtime::Activation& activation,
      std::unique_ptr<const AttributeMatcher> matcher);

  static const AttributeMatcher* absl_nullable GetAttributeMatcher(
      const google::api::expr::runtime::BaseActivation& activation);

  static void SetAttributeMatcher(Activation& activation,
                                  const AttributeMatcher* matcher);

  static void SetAttributeMatcher(
      Activation& activation, std::unique_ptr<const AttributeMatcher> matcher);

  static const AttributeMatcher* absl_nullable GetAttributeMatcher(
      const ActivationInterface& activation);
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ACTIVATION_MATCHER_ACCESS_H_
