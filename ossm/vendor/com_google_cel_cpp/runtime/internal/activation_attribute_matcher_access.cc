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

#include "runtime/internal/activation_attribute_matcher_access.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "eval/public/activation.h"
#include "runtime/activation.h"
#include "runtime/internal/attribute_matcher.h"

namespace cel::runtime_internal {

void ActivationAttributeMatcherAccess::SetAttributeMatcher(
    google::api::expr::runtime::Activation& activation,
    const AttributeMatcher* matcher) {
  activation.SetAttributeMatcher(matcher);
}

void ActivationAttributeMatcherAccess::SetAttributeMatcher(
    google::api::expr::runtime::Activation& activation,
    std::unique_ptr<const AttributeMatcher> matcher) {
  activation.SetAttributeMatcher(std::move(matcher));
}

const AttributeMatcher* absl_nullable
ActivationAttributeMatcherAccess::GetAttributeMatcher(
    const google::api::expr::runtime::BaseActivation& activation) {
  return activation.GetAttributeMatcher();
}

void ActivationAttributeMatcherAccess::SetAttributeMatcher(
    Activation& activation, const AttributeMatcher* matcher) {
  activation.SetAttributeMatcher(matcher);
}

void ActivationAttributeMatcherAccess::SetAttributeMatcher(
    Activation& activation, std::unique_ptr<const AttributeMatcher> matcher) {
  activation.SetAttributeMatcher(std::move(matcher));
}

const AttributeMatcher* absl_nullable
ActivationAttributeMatcherAccess::GetAttributeMatcher(
    const ActivationInterface& activation) {
  return activation.GetAttributeMatcher();
}

}  // namespace cel::runtime_internal
