// Copyright 2025 Google LLC
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

#include "extensions/regex_ext.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/internal/runtime_friend_access.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime_builder.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "re2/re2.h"

namespace cel::extensions {
namespace {

using ::cel::checker_internal::BuiltinsArena;

Value Extract(const StringValue& target, const StringValue& regex,
              const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
              google::protobuf::MessageFactory* absl_nonnull message_factory,
              google::protobuf::Arena* absl_nonnull arena) {
  std::string target_scratch;
  std::string regex_scratch;
  absl::string_view target_view = target.ToStringView(&target_scratch);
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("given regex is invalid: %s", re2.error())));
  }
  const int group_count = re2.NumberOfCapturingGroups();
  if (group_count > 1) {
    return ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
        "regular expression has more than one capturing group: %s",
        regex_view)));
  }

  // Space for the full match (\0) and the first capture group (\1).
  absl::string_view submatches[2];
  if (re2.Match(target_view, 0, target_view.length(), RE2::UNANCHORED,
                submatches, 2)) {
    // Return the capture group if it exists else return the full match.
    const absl::string_view result_view =
        (group_count == 1) ? submatches[1] : submatches[0];
    return OptionalValue::Of(StringValue::From(result_view, arena), arena);
  }

  return OptionalValue::None();
}

Value ExtractAll(const StringValue& target, const StringValue& regex,
                 const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                 google::protobuf::MessageFactory* absl_nonnull message_factory,
                 google::protobuf::Arena* absl_nonnull arena) {
  std::string target_scratch;
  std::string regex_scratch;
  absl::string_view target_view = target.ToStringView(&target_scratch);
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("given regex is invalid: %s", re2.error())));
  }
  const int group_count = re2.NumberOfCapturingGroups();
  if (group_count > 1) {
    return ErrorValue(absl::InvalidArgumentError(absl::StrFormat(
        "regular expression has more than one capturing group: %s",
        regex_view)));
  }

  auto builder = NewListValueBuilder(arena);
  absl::string_view temp_target = target_view;

  // Space for the full match (\0) and the first capture group (\1).
  absl::string_view submatches[2];
  const int group_to_extract = (group_count == 1) ? 1 : 0;

  while (re2.Match(temp_target, 0, temp_target.length(), RE2::UNANCHORED,
                   submatches, group_count + 1)) {
    const absl::string_view& full_match = submatches[0];
    const absl::string_view& desired_capture = submatches[group_to_extract];

    // Avoid infinite loops on zero-length matches
    if (full_match.empty()) {
      if (temp_target.empty()) {
        break;
      }
      temp_target.remove_prefix(1);
      continue;
    }

    if (group_count == 1 && desired_capture.empty()) {
      temp_target.remove_prefix(full_match.data() - temp_target.data() +
                                full_match.length());
      continue;
    }

    absl::Status status =
        builder->Add(StringValue::From(desired_capture, arena));
    if (!status.ok()) {
      return ErrorValue(status);
    }
    temp_target.remove_prefix(full_match.data() - temp_target.data() +
                              full_match.length());
  }

  return std::move(*builder).Build();
}

Value ReplaceAll(const StringValue& target, const StringValue& regex,
                 const StringValue& replacement,
                 const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                 google::protobuf::MessageFactory* absl_nonnull message_factory,
                 google::protobuf::Arena* absl_nonnull arena) {
  std::string target_scratch;
  std::string regex_scratch;
  std::string replacement_scratch;
  absl::string_view target_view = target.ToStringView(&target_scratch);
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  absl::string_view replacement_view =
      replacement.ToStringView(&replacement_scratch);
  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("given regex is invalid: %s", re2.error())));
  }

  std::string error_string;
  if (!re2.CheckRewriteString(replacement_view, &error_string)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("invalid replacement string: %s", error_string)));
  }

  std::string output(target_view);
  RE2::GlobalReplace(&output, re2, replacement_view);

  return StringValue::From(std::move(output), arena);
}

Value ReplaceN(const StringValue& target, const StringValue& regex,
               const StringValue& replacement, int64_t count,
               const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
               google::protobuf::MessageFactory* absl_nonnull message_factory,
               google::protobuf::Arena* absl_nonnull arena) {
  if (count == 0) {
    return target;
  }
  if (count < 0) {
    return ReplaceAll(target, regex, replacement, descriptor_pool,
                      message_factory, arena);
  }

  std::string target_scratch;
  std::string regex_scratch;
  std::string replacement_scratch;
  absl::string_view target_view = target.ToStringView(&target_scratch);
  absl::string_view regex_view = regex.ToStringView(&regex_scratch);
  absl::string_view replacement_view =
      replacement.ToStringView(&replacement_scratch);
  RE2 re2(regex_view);
  if (!re2.ok()) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("given regex is invalid: %s", re2.error())));
  }
  std::string error_string;
  if (!re2.CheckRewriteString(replacement_view, &error_string)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrFormat("invalid replacement string: %s", error_string)));
  }

  std::string output;
  absl::string_view temp_target = target_view;
  int replaced_count = 0;
  // RE2's Rewrite only supports substitutions for groups \0 through \9.
  absl::string_view match[10];
  int nmatch = std::min(9, re2.NumberOfCapturingGroups()) + 1;

  while (replaced_count < count &&
         re2.Match(temp_target, 0, temp_target.length(), RE2::UNANCHORED, match,
                   nmatch)) {
    absl::string_view full_match = match[0];

    output.append(temp_target.data(), full_match.data() - temp_target.data());

    if (!re2.Rewrite(&output, replacement_view, match, nmatch)) {
      // This should ideally not happen given CheckRewriteString passed
      return ErrorValue(absl::InternalError("rewrite failed unexpectedly"));
    }

    temp_target.remove_prefix(full_match.data() - temp_target.data() +
                              full_match.length());
    replaced_count++;
  }

  output.append(temp_target.data(), temp_target.length());

  return StringValue::From(std::move(output), arena);
}

absl::Status RegisterRegexExtensionFunctions(FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue>::
           RegisterGlobalOverload("regex.extract", &Extract, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue>::
           RegisterGlobalOverload("regex.extractAll", &ExtractAll, registry)));
  CEL_RETURN_IF_ERROR(
      (TernaryFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue,
          StringValue>::RegisterGlobalOverload("regex.replace", &ReplaceAll,
                                               registry)));
  CEL_RETURN_IF_ERROR(
      (QuaternaryFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue, StringValue,
          int64_t>::RegisterGlobalOverload("regex.replace", &ReplaceN,
                                           registry)));
  return absl::OkStatus();
}

const Type& OptionalStringType() {
  static absl::NoDestructor<Type> kInstance(
      OptionalType(BuiltinsArena(), StringType()));
  return *kInstance;
}

const Type& ListStringType() {
  static absl::NoDestructor<Type> kInstance(
      ListType(BuiltinsArena(), StringType()));
  return *kInstance;
}

absl::Status RegisterRegexCheckerDecls(TypeCheckerBuilder& builder) {
  CEL_ASSIGN_OR_RETURN(
      FunctionDecl extract_decl,
      MakeFunctionDecl(
          "regex.extract",
          MakeOverloadDecl("regex_extract_string_string", OptionalStringType(),
                           StringType(), StringType())));

  CEL_ASSIGN_OR_RETURN(
      FunctionDecl extract_all_decl,
      MakeFunctionDecl(
          "regex.extractAll",
          MakeOverloadDecl("regex_extractAll_string_string", ListStringType(),
                           StringType(), StringType())));

  CEL_ASSIGN_OR_RETURN(
      FunctionDecl replace_decl,
      MakeFunctionDecl(
          "regex.replace",
          MakeOverloadDecl("regex_replace_string_string_string", StringType(),
                           StringType(), StringType(), StringType()),
          MakeOverloadDecl("regex_replace_string_string_string_int",
                           StringType(), StringType(), StringType(),
                           StringType(), IntType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(extract_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(extract_all_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(replace_decl));
  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterRegexExtensionFunctions(RuntimeBuilder& builder) {
  auto& runtime = cel::internal::down_cast<runtime_internal::RuntimeImpl&>(
      runtime_internal::RuntimeFriendAccess::GetMutableRuntime(builder));
  if (!runtime.expr_builder().optional_types_enabled()) {
    return absl::InvalidArgumentError(
        "regex extensions requires the optional types to be enabled");
  }
  if (runtime.expr_builder().options().enable_regex) {
    CEL_RETURN_IF_ERROR(
        RegisterRegexExtensionFunctions(builder.function_registry()));
  }
  return absl::OkStatus();
}

absl::Status RegisterRegexExtensionFunctions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options) {
  if (!options.enable_regex) {
    return RegisterRegexExtensionFunctions(registry->InternalGetRegistry());
  }
  return absl::OkStatus();
}

CheckerLibrary RegexExtCheckerLibrary() {
  return {.id = "cel.lib.ext.regex", .configure = RegisterRegexCheckerDecls};
}

CompilerLibrary RegexExtCompilerLibrary() {
  return CompilerLibrary::FromCheckerLibrary(RegexExtCheckerLibrary());
}

}  // namespace cel::extensions
