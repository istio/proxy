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
#include "runtime/standard/regex_functions.h"

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "re2/re2.h"

namespace cel {
namespace {}  // namespace

absl::Status RegisterRegexFunctions(FunctionRegistry& registry,
                                    const RuntimeOptions& options) {
  if (options.enable_regex) {
    auto regex_matches = [max_size = options.regex_max_program_size](
                             const StringValue& target,
                             const StringValue& regex) -> Value {
      RE2 re2(regex.ToString());
      if (max_size > 0 && re2.ProgramSize() > max_size) {
        return ErrorValue(
            absl::InvalidArgumentError("exceeded RE2 max program size"));
      }
      if (!re2.ok()) {
        return ErrorValue(
            absl::InvalidArgumentError("invalid regex for match"));
      }
      return BoolValue(RE2::PartialMatch(target.ToString(), re2));
    };

    // bind str.matches(re) and matches(str, re)
    for (bool receiver_style : {true, false}) {
      using MatchFnAdapter =
          BinaryFunctionAdapter<Value, const StringValue&, const StringValue&>;
      CEL_RETURN_IF_ERROR(
          registry.Register(MatchFnAdapter::CreateDescriptor(
                                cel::builtin::kRegexMatch, receiver_style),
                            MatchFnAdapter::WrapFunction(regex_matches)));
    }
  }  // if options.enable_regex

  return absl::OkStatus();
}

}  // namespace cel
