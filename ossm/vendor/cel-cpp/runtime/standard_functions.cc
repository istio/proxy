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

#include "runtime/standard_functions.h"

#include "absl/status/status.h"
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

namespace cel {

absl::Status RegisterStandardFunctions(FunctionRegistry& registry,
                                       const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterContainerFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterContainerMembershipFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterLogicalFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterRegexFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterStringFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterTimeFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterEqualityFunctions(registry, options));

  return RegisterTypeConversionFunctions(registry, options);
}

}  // namespace cel
