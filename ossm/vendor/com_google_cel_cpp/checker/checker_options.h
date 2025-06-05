// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_CHECKER_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_CHECKER_OPTIONS_H_

namespace cel {

// Options for enabling core type checker features.
struct CheckerOptions {
  // Enable overloads for numeric comparisons across types.
  // For example, 1.0 < 2 will resolve to lt_double_int.
  //
  // By default, this is disabled and expressions must explicitly cast to dyn or
  // the same type to compare.
  bool enable_cross_numeric_comparisons = false;

  // Enable legacy behavior for null assignment.
  //
  // Historically, CEL has allowed null to be assigned to structs, abstract
  // types, durations, timestamps, and any types. This is inconsistent with
  // CEL's usual interpretation of null as a literal JSON null.
  //
  // TODO: Need a concrete plan for updating existing CEL
  // expressions that depend on the old behavior.
  bool enable_legacy_null_assignment = true;

  // Enable updating parsed struct type names to the fully qualified type name
  // when resolved.
  //
  // Enabled by default, but can be disabled to preserve the original type name
  // as parsed.
  bool update_struct_type_names = true;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_CHECKER_OPTIONS_H_
