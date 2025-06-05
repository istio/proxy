// Copyright 2023 Google LLC
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

#include "eval/public/cel_options.h"

#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

cel::RuntimeOptions ConvertToRuntimeOptions(const InterpreterOptions& options) {
  return cel::RuntimeOptions{/*.container=*/"",
                             options.unknown_processing,
                             options.enable_missing_attribute_errors,
                             options.enable_timestamp_duration_overflow_errors,
                             options.short_circuiting,
                             options.enable_comprehension,
                             options.comprehension_max_iterations,
                             options.enable_comprehension_list_append,
                             options.enable_regex,
                             options.regex_max_program_size,
                             options.enable_string_conversion,
                             options.enable_string_concat,
                             options.enable_list_concat,
                             options.enable_list_contains,
                             options.fail_on_warnings,
                             options.enable_qualified_type_identifiers,
                             options.enable_heterogeneous_equality,
                             options.enable_empty_wrapper_null_unboxing,
                             options.enable_lazy_bind_initialization,
                             options.max_recursion_depth,
                             options.enable_recursive_tracing,
                             options.use_legacy_container_builders};
}

}  // namespace google::api::expr::runtime
