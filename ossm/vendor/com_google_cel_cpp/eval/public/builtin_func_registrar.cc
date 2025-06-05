// Copyright 2021 Google LLC
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

#include "eval/public/builtin_func_registrar.h"

#include "absl/status/status.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/standard/arithmetic_functions.h"
#include "runtime/standard/comparison_functions.h"
#include "runtime/standard/container_functions.h"
#include "runtime/standard/container_membership_functions.h"
#include "runtime/standard/equality_functions.h"
#include "runtime/standard/logical_functions.h"
#include "runtime/standard/regex_functions.h"
#include "runtime/standard/string_functions.h"
#include "runtime/standard/time_functions.h"
#include "runtime/standard/type_conversion_functions.h"

namespace google::api::expr::runtime {

absl::Status RegisterBuiltinFunctions(CelFunctionRegistry* registry,
                                      const InterpreterOptions& options) {
  cel::FunctionRegistry& modern_registry = registry->InternalGetRegistry();
  cel::RuntimeOptions runtime_options = ConvertToRuntimeOptions(options);

  CEL_RETURN_IF_ERROR(
      cel::RegisterLogicalFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterComparisonFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterContainerFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(cel::RegisterContainerMembershipFunctions(
      modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterTypeConversionFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterArithmeticFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterTimeFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterStringFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterRegexFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterEqualityFunctions(modern_registry, runtime_options));

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
