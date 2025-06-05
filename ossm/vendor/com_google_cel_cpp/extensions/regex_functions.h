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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_REGEX_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_REGEX_FUNCTIONS_H_

#include "absl/status/status.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"

namespace cel::extensions {

constexpr absl::string_view kRegexExtract = "re.extract";
constexpr absl::string_view kRegexCapture = "re.capture";
constexpr absl::string_view kRegexCaptureN = "re.captureN";

// Register Extract and Capture Functions for RE2
// Requires options.enable_regex to be true
absl::Status RegisterRegexFunctions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options);
}  // namespace cel::extensions
#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_REGEX_FUNCTIONS_H_
