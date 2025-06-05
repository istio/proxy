/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_OPTIONS_H_

#include <string>

#include "absl/base/attributes.h"

namespace cel {

// Options for unknown processing.
enum class UnknownProcessingOptions {
  // No unknown processing.
  kDisabled,
  // Only attributes supported.
  kAttributeOnly,
  // Attributes and functions supported. Function results are dependent on the
  // logic for handling unknown_attributes, so clients must opt in to both.
  kAttributeAndFunction
};

// Options for handling unset wrapper types on field access.
enum class ProtoWrapperTypeOptions {
  // Default: legacy behavior following proto semantics (unset behaves as though
  // it is set to default value).
  kUnsetProtoDefault,
  // CEL spec behavior, unset wrapper is treated as a null value when accessed.
  kUnsetNull,
};

// LINT.IfChange
// Interpreter options for controlling evaluation and builtin functions.
//
// Members should provide simple parameters for configuring core features and
// built-ins.
//
// Optimizations or features that have a heavy footprint should be added via an
// extension API.
struct RuntimeOptions {
  // Default container for resolving variables, types, and functions.
  // Follows protobuf namespace rules.
  std::string container = "";

  // Level of unknown support enabled.
  UnknownProcessingOptions unknown_processing =
      UnknownProcessingOptions::kDisabled;

  bool enable_missing_attribute_errors = false;

  // Enable timestamp duration overflow checks.
  //
  // The CEL-Spec indicates that overflow should occur outside the range of
  // string-representable timestamps, and at the limit of durations which can be
  // expressed with a single int64_t value.
  bool enable_timestamp_duration_overflow_errors = false;

  // Enable short-circuiting of the logical operator evaluation. If enabled,
  // AND, OR, and TERNARY do not evaluate the entire expression once the the
  // resulting value is known from the left-hand side.
  bool short_circuiting = true;

  // Enable comprehension expressions (e.g. exists, all)
  bool enable_comprehension = true;

  // Set maximum number of iterations in the comprehension expressions if
  // comprehensions are enabled. The limit applies globally per an evaluation,
  // including the nested loops as well. Use value 0 to disable the upper bound.
  int comprehension_max_iterations = 10000;

  // Enable list append within comprehensions. Note, this option is not safe
  // with hand-rolled ASTs.
  bool enable_comprehension_list_append = false;

  // Enable RE2 match() overload.
  bool enable_regex = true;

  // Set maximum program size for RE2 regex if regex overload is enabled.
  // Evaluates to an error if a regex exceeds it. Use value 0 to disable the
  // upper bound.
  int regex_max_program_size = 0;

  // Enable string() overloads.
  bool enable_string_conversion = true;

  // Enable string concatenation overload.
  bool enable_string_concat = true;

  // Enable list concatenation overload.
  bool enable_list_concat = true;

  // Enable list membership overload.
  bool enable_list_contains = true;

  // Treat builder warnings as fatal errors.
  bool fail_on_warnings = true;

  // Enable the resolution of qualified type identifiers as type values instead
  // of field selections.
  //
  // This toggle may cause certain identifiers which overlap with CEL built-in
  // type or with protobuf message types linked into the binary to be resolved
  // as static type values rather than as per-eval variables.
  bool enable_qualified_type_identifiers = false;

  // Enable heterogeneous comparisons (e.g. support for cross-type comparisons).
  ABSL_DEPRECATED(
      "The ability to disable heterogeneous equality is being removed in the "
      "near future")
  bool enable_heterogeneous_equality = true;

  // Enables unwrapping proto wrapper types to null if unset. e.g. if an
  // expression access a field of type google.protobuf.Int64Value that is unset,
  // that will result in a Null cel value, as opposed to returning the
  // cel representation of the proto defined default int64_t: 0.
  bool enable_empty_wrapper_null_unboxing = false;

  // Enable lazy cel.bind alias initialization.
  //
  // This is now always enabled. Setting this option has no effect. It will be
  // removed in a later update.
  bool enable_lazy_bind_initialization = true;

  // Maximum recursion depth for evaluable programs.
  //
  // This is proportional to the maximum number of recursive Evaluate calls that
  // a single expression program might require while evaluating. This is
  // coarse -- the actual C++ stack requirements will vary depending on the
  // expression.
  //
  // This does not account for re-entrant evaluation in a client's extension
  // function.
  //
  // -1 means unbounded.
  int max_recursion_depth = 0;

  // Enable tracing support for recursively planned programs.
  //
  // Unlike the stack machine implementation, supporting tracing can affect
  // performance whether or not tracing is requested for a given evaluation.
  bool enable_recursive_tracing = false;

  // Use legacy containers for lists and maps when possible.
  //
  // For interoperating with legacy APIs, it can be more efficient to maintain
  // the list/map representation as CelValues. Requires using an Arena,
  // otherwise modern implementations are used.
  //
  // Default is false for the modern option type.
  bool use_legacy_container_builders = false;
};
// LINT.ThenChange(//depot/google3/eval/public/cel_options.h)

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_OPTIONS_H_
