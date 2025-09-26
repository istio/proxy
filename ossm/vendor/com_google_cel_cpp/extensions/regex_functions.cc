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

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "re2/re2.h"

namespace cel::extensions {
namespace {

using ::cel::checker_internal::BuiltinsArena;
using ::google::api::expr::runtime::CelFunctionRegistry;
using ::google::api::expr::runtime::InterpreterOptions;

// Extract matched group values from the given target string and rewrite the
// string
Value ExtractString(const StringValue& target, const StringValue& regex,
                    const StringValue& rewrite,
                    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena) {
  std::string regex_scratch;
  std::string target_scratch;
  std::string rewrite_scratch;
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  absl::string_view target_view = target.ToStringView(&target_scratch);
  absl::string_view rewrite_view = rewrite.ToStringView(&rewrite_scratch);

  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError("Given Regex is Invalid"));
  }
  std::string output;
  bool result = RE2::Extract(target_view, re2, rewrite_view, &output);
  if (!result) {
    return ErrorValue(absl::InvalidArgumentError(
        "Unable to extract string for the given regex"));
  }
  return StringValue::From(std::move(output), arena);
}

// Captures the first unnamed/named group value
// NOTE: For capturing all the groups, use CaptureStringN instead
Value CaptureString(const StringValue& target, const StringValue& regex,
                    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena) {
  std::string regex_scratch;
  std::string target_scratch;
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  absl::string_view target_view = target.ToStringView(&target_scratch);
  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError("Given Regex is Invalid"));
  }
  std::string output;
  bool result = RE2::FullMatch(target_view, re2, &output);
  if (!result) {
    return ErrorValue(absl::InvalidArgumentError(
        "Unable to capture groups for the given regex"));
  } else {
    return StringValue::From(std::move(output), arena);
  }
}

// Does a FullMatchN on the given string and regex and returns a map with <key,
// value> pairs as follows:
//   a. For a named group - <named_group_name, captured_string>
//   b. For an unnamed group - <group_index, captured_string>
absl::StatusOr<Value> CaptureStringN(
    const StringValue& target, const StringValue& regex,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string target_scratch;
  std::string regex_scratch;
  absl::string_view target_view = target.ToStringView(&target_scratch);
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError("Given Regex is Invalid"));
  }
  const int capturing_groups_count = re2.NumberOfCapturingGroups();
  const auto& named_capturing_groups_map = re2.CapturingGroupNames();
  if (capturing_groups_count <= 0) {
    return ErrorValue(absl::InvalidArgumentError(
        "Capturing groups were not found in the given regex."));
  }
  std::vector<std::string> captured_strings(capturing_groups_count);
  std::vector<RE2::Arg> captured_string_addresses(capturing_groups_count);
  std::vector<RE2::Arg*> argv(capturing_groups_count);
  for (int j = 0; j < capturing_groups_count; j++) {
    captured_string_addresses[j] = &captured_strings[j];
    argv[j] = &captured_string_addresses[j];
  }
  bool result =
      RE2::FullMatchN(target_view, re2, argv.data(), capturing_groups_count);
  if (!result) {
    return ErrorValue(absl::InvalidArgumentError(
        "Unable to capture groups for the given regex"));
  }
  auto builder = cel::NewMapValueBuilder(arena);
  builder->Reserve(capturing_groups_count);
  for (int index = 1; index <= capturing_groups_count; index++) {
    auto it = named_capturing_groups_map.find(index);
    std::string name = it != named_capturing_groups_map.end()
                           ? it->second
                           : std::to_string(index);
    CEL_RETURN_IF_ERROR(builder->Put(
        StringValue::From(std::move(name), arena),
        StringValue::From(std::move(captured_strings[index - 1]), arena)));
  }
  return std::move(*builder).Build();
}

absl::Status RegisterRegexFunctions(FunctionRegistry& registry) {
  // Register Regex Extract Function
  CEL_RETURN_IF_ERROR(
      (TernaryFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue,
          StringValue>::RegisterGlobalOverload(kRegexExtract, &ExtractString,
                                               registry)));

  // Register Regex Captures Function
  CEL_RETURN_IF_ERROR((
      BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue,
                            StringValue>::RegisterGlobalOverload(kRegexCapture,
                                                                 &CaptureString,
                                                                 registry)));

  // Register Regex CaptureN Function
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue>::
           RegisterGlobalOverload(kRegexCaptureN, &CaptureStringN, registry)));
  return absl::OkStatus();
}

const Type& CaptureNMapType() {
  static absl::NoDestructor<Type> kInstance(
      MapType(BuiltinsArena(), StringType(), StringType()));
  return *kInstance;
}

absl::Status RegisterRegexDecls(TypeCheckerBuilder& builder) {
  CEL_ASSIGN_OR_RETURN(
      FunctionDecl regex_extract_decl,
      MakeFunctionDecl(
          std::string(kRegexExtract),
          MakeOverloadDecl("re_extract_string_string_string", StringType(),
                           StringType(), StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(regex_extract_decl));

  CEL_ASSIGN_OR_RETURN(
      FunctionDecl regex_capture_decl,
      MakeFunctionDecl(
          std::string(kRegexCapture),
          MakeOverloadDecl("re_capture_string_string", StringType(),
                           StringType(), StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(regex_capture_decl));

  CEL_ASSIGN_OR_RETURN(
      FunctionDecl regex_capture_n_decl,
      MakeFunctionDecl(
          std::string(kRegexCaptureN),
          MakeOverloadDecl("re_captureN_string_string", CaptureNMapType(),
                           StringType(), StringType())));
  return builder.AddFunction(regex_capture_n_decl);
}

}  // namespace

absl::Status RegisterRegexFunctions(FunctionRegistry& registry,
                                    const RuntimeOptions& options) {
  if (options.enable_regex) {
    CEL_RETURN_IF_ERROR(RegisterRegexFunctions(registry));
  }
  return absl::OkStatus();
}

absl::Status RegisterRegexFunctions(CelFunctionRegistry* registry,
                                    const InterpreterOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterRegexFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options)));
  return absl::OkStatus();
}

CheckerLibrary RegexCheckerLibrary() {
  return {.id = "cpp_regex", .configure = RegisterRegexDecls};
}

}  // namespace cel::extensions
