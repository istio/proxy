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

#include "eval/public/comparison_functions.h"

#include "absl/status/status.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/standard/comparison_functions.h"

namespace google::api::expr::runtime {

absl::Status RegisterComparisonFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  cel::RuntimeOptions modern_options = ConvertToRuntimeOptions(options);
  cel::FunctionRegistry& modern_registry = registry->InternalGetRegistry();
  return cel::RegisterComparisonFunctions(modern_registry, modern_options);
}

}  // namespace google::api::expr::runtime
