// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_STANDARD_MACROS_H_
#define THIRD_PARTY_CEL_CPP_PARSER_STANDARD_MACROS_H_

#include "absl/status/status.h"
#include "parser/macro_registry.h"
#include "parser/options.h"

namespace cel {

// Registers the standard macros defined by the Common Expression Language.
// https://github.com/google/cel-spec/blob/master/doc/langdef.md#macros
absl::Status RegisterStandardMacros(MacroRegistry& registry,
                                    const ParserOptions& options);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_PARSER_STANDARD_MACROS_H_
