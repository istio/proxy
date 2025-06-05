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

#include "extensions/regex_functions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "re2/re2.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::runtime::CelFunction;
using ::google::api::expr::runtime::CelFunctionRegistry;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateErrorValue;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::PortableBinaryFunctionAdapter;
using ::google::api::expr::runtime::PortableFunctionAdapter;
using ::google::protobuf::Arena;

// Extract matched group values from the given target string and rewrite the
// string
CelValue ExtractString(Arena* arena, CelValue::StringHolder target,
                       CelValue::StringHolder regex,
                       CelValue::StringHolder rewrite) {
  RE2 re2(regex.value());
  if (!re2.ok()) {
    return CreateErrorValue(
        arena, absl::InvalidArgumentError("Given Regex is Invalid"));
  }
  std::string output;
  auto result = RE2::Extract(target.value(), re2, rewrite.value(), &output);
  if (!result) {
    return CreateErrorValue(
        arena, absl::InvalidArgumentError(
                   "Unable to extract string for the given regex"));
  }
  return CelValue::CreateString(
      google::protobuf::Arena::Create<std::string>(arena, output));
}

// Captures the first unnamed/named group value
// NOTE: For capturing all the groups, use CaptureStringN instead
CelValue CaptureString(Arena* arena, CelValue::StringHolder target,
                       CelValue::StringHolder regex) {
  RE2 re2(regex.value());
  if (!re2.ok()) {
    return CreateErrorValue(
        arena, absl::InvalidArgumentError("Given Regex is Invalid"));
  }
  std::string output;
  auto result = RE2::FullMatch(target.value(), re2, &output);
  if (!result) {
    return CreateErrorValue(
        arena, absl::InvalidArgumentError(
                   "Unable to capture groups for the given regex"));
  } else {
    auto cel_value = CelValue::CreateString(
        google::protobuf::Arena::Create<std::string>(arena, output));
    return cel_value;
  }
}

// Does a FullMatchN on the given string and regex and returns a map with <key,
// value> pairs as follows:
//   a. For a named group - <named_group_name, captured_string>
//   b. For an unnamed group - <group_index, captured_string>
CelValue CaptureStringN(Arena* arena, CelValue::StringHolder target,
                        CelValue::StringHolder regex) {
  RE2 re2(regex.value());
  if (!re2.ok()) {
    return CreateErrorValue(
        arena, absl::InvalidArgumentError("Given Regex is Invalid"));
  }
  const int capturing_groups_count = re2.NumberOfCapturingGroups();
  const auto& named_capturing_groups_map = re2.CapturingGroupNames();
  if (capturing_groups_count <= 0) {
    return CreateErrorValue(arena,
                            absl::InvalidArgumentError(
                                "Capturing groups were not found in the given "
                                "regex."));
  }
  std::vector<std::string> captured_strings(capturing_groups_count);
  std::vector<RE2::Arg> captured_string_addresses(capturing_groups_count);
  std::vector<RE2::Arg*> argv(capturing_groups_count);
  for (int j = 0; j < capturing_groups_count; j++) {
    captured_string_addresses[j] = &captured_strings[j];
    argv[j] = &captured_string_addresses[j];
  }
  auto result =
      RE2::FullMatchN(target.value(), re2, argv.data(), capturing_groups_count);
  if (!result) {
    return CreateErrorValue(
        arena, absl::InvalidArgumentError(
                   "Unable to capture groups for the given regex"));
  }
  std::vector<std::pair<CelValue, CelValue>> cel_values;
  for (int index = 1; index <= capturing_groups_count; index++) {
    auto it = named_capturing_groups_map.find(index);
    std::string name = it != named_capturing_groups_map.end()
                           ? it->second
                           : std::to_string(index);
    cel_values.emplace_back(
        CelValue::CreateString(google::protobuf::Arena::Create<std::string>(arena, name)),
        CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
            arena, captured_strings[index - 1])));
  }
  auto container_map = google::api::expr::runtime::CreateContainerBackedMap(
      absl::MakeSpan(cel_values));

  // Release ownership of container_map to Arena.
  ::google::api::expr::runtime::CelMap* cel_map = container_map->release();
  arena->Own(cel_map);
  return CelValue::CreateMap(cel_map);
}

absl::Status RegisterRegexFunctions(CelFunctionRegistry* registry) {
  // Register Regex Extract Function
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<CelValue, CelValue::StringHolder,
                               CelValue::StringHolder, CelValue::StringHolder>::
           CreateAndRegister(
               kRegexExtract, /*receiver_type=*/false,
               [](Arena* arena, CelValue::StringHolder target,
                  CelValue::StringHolder regex,
                  CelValue::StringHolder rewrite) -> CelValue {
                 return ExtractString(arena, target, regex, rewrite);
               },
               registry)));

  // Register Regex Captures Function
  CEL_RETURN_IF_ERROR(registry->Register(
      PortableBinaryFunctionAdapter<CelValue, CelValue::StringHolder,
                                    CelValue::StringHolder>::
          Create(kRegexCapture, /*receiver_style=*/false,
                 [](Arena* arena, CelValue::StringHolder target,
                    CelValue::StringHolder regex) -> CelValue {
                   return CaptureString(arena, target, regex);
                 })));

  // Register Regex CaptureN Function
  return registry->Register(
      PortableBinaryFunctionAdapter<CelValue, CelValue::StringHolder,
                                    CelValue::StringHolder>::
          Create(kRegexCaptureN, /*receiver_style=*/false,
                 [](Arena* arena, CelValue::StringHolder target,
                    CelValue::StringHolder regex) -> CelValue {
                   return CaptureStringN(arena, target, regex);
                 }));
}

}  // namespace

absl::Status RegisterRegexFunctions(CelFunctionRegistry* registry,
                                    const InterpreterOptions& options) {
  if (options.enable_regex) {
    CEL_RETURN_IF_ERROR(RegisterRegexFunctions(registry));
  }
  return absl::OkStatus();
}

}  // namespace cel::extensions
