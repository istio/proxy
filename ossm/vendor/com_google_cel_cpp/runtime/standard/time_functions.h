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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_TIME_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_TIME_FUNCTIONS_H_

#include "absl/status/status.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {

// Register builtin timestamp and duration functions:
//
// (timestamp).getFullYear(<timezone:string>) -> int
// (timestamp).getMonth(<timezone:string>) -> int
// (timestamp).getDayOfYear(<timezone:string>) -> int
// (timestamp).getDayOfMonth(<timezone:string>) -> int
// (timestamp).getDayOfWeek(<timezone:string>) -> int
// (timestamp).getDate(<timezone:string>) -> int
// (timestamp).getHours(<timezone:string>) -> int
// (timestamp).getMinutes(<timezone:string>) -> int
// (timestamp).getSeconds(<timezone:string>) -> int
// (timestamp).getMilliseconds(<timezone:string>) -> int
//
// (duration).getHours() -> int
// (duration).getMinutes() -> int
// (duration).getSeconds() -> int
// (duration).getMilliseconds() -> int
//
// _+_(timestamp, duration) -> timestamp
// _+_(duration, timestamp) -> timestamp
// _+_(duration, duration) -> duration
// _-_(timestamp, timestamp) -> duration
// _-_(timestamp, duration) -> timestamp
// _-_(duration, duration) -> duration
//
// Most users should use RegisterBuiltinFunctions, which includes these
// definitions.
absl::Status RegisterTimeFunctions(FunctionRegistry& registry,
                                   const RuntimeOptions& options);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_TIME_FUNCTIONS_H_
