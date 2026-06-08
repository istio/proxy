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

#include "extensions/strings.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/internal/builtins_arena.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "extensions/formatting.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

namespace {

using ::cel::checker_internal::BuiltinsArena;

struct AppendToStringVisitor {
  std::string& append_to;

  void operator()(absl::string_view string) const { append_to.append(string); }

  void operator()(const absl::Cord& cord) const {
    append_to.append(static_cast<std::string>(cord));
  }
};

absl::StatusOr<Value> Join2(
    const ListValue& value, const StringValue& separator,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return separator.Join(value, descriptor_pool, message_factory, arena);
}

absl::StatusOr<Value> Join1(
    const ListValue& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return StringValue().Join(value, descriptor_pool, message_factory, arena);
}

absl::StatusOr<Value> Split3(
    const StringValue& string, const StringValue& delimiter, int64_t limit,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return string.Split(delimiter, limit, arena);
}

absl::StatusOr<Value> Split2(
    const StringValue& string, const StringValue& delimiter,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return string.Split(delimiter, arena);
}

absl::StatusOr<Value> Replace2(const StringValue& string,
                               const StringValue& old_sub,
                               const StringValue& new_sub, int64_t limit,
                               const google::protobuf::DescriptorPool* absl_nonnull,
                               google::protobuf::MessageFactory* absl_nonnull,
                               google::protobuf::Arena* absl_nonnull arena) {
  return string.Replace(old_sub, new_sub, limit, arena);
}

absl::StatusOr<Value> Replace1(
    const StringValue& string, const StringValue& old_sub,
    const StringValue& new_sub,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return string.Replace(old_sub, new_sub, -1, arena);
}

Value CharAt(const StringValue& string, int64_t pos) {
  return string.CharAt(pos);
}

int64_t IndexOf2(const StringValue& haystack, const StringValue& needle) {
  return haystack.IndexOf(needle).value_or(-1);
}

Value IndexOf3(const StringValue& haystack, const StringValue& needle,
               int64_t pos) {
  if (pos > haystack.Size()) {
    return ErrorValue{
        absl::InvalidArgumentError(absl::StrCat("index out of range: ", pos))};
  }
  return IntValue(haystack.IndexOf(needle, pos).value_or(-1));
}

int64_t LastIndexOf2(const StringValue& haystack, const StringValue& needle) {
  return haystack.LastIndexOf(needle).value_or(-1);
}

Value LastIndexOf3(const StringValue& haystack, const StringValue& needle,
                   int64_t pos) {
  if (pos < 0 || pos > haystack.Size()) {
    return ErrorValue{
        absl::InvalidArgumentError(absl::StrCat("index out of range: ", pos))};
  }
  return IntValue(haystack.LastIndexOf(needle, pos).value_or(-1));
}

Value Substring2(const StringValue& string, int64_t start) {
  return string.Substring(start);
}

Value Substring3(const StringValue& string, int64_t start, int64_t end) {
  return string.Substring(start, end);
}

StringValue Trim(const StringValue& string) { return string.Trim(); }

StringValue LowerAscii(const StringValue& string,
                       const google::protobuf::DescriptorPool* absl_nonnull,
                       google::protobuf::MessageFactory* absl_nonnull,
                       google::protobuf::Arena* absl_nonnull arena) {
  return string.LowerAscii(arena);
}

StringValue UpperAscii(const StringValue& string,
                       const google::protobuf::DescriptorPool* absl_nonnull,
                       google::protobuf::MessageFactory* absl_nonnull,
                       google::protobuf::Arena* absl_nonnull arena) {
  return string.UpperAscii(arena);
}

StringValue Quote(const StringValue& string,
                  const google::protobuf::DescriptorPool* absl_nonnull,
                  google::protobuf::MessageFactory* absl_nonnull,
                  google::protobuf::Arena* absl_nonnull arena) {
  return string.Quote(arena);
}

StringValue Reverse(const StringValue& string,
                    const google::protobuf::DescriptorPool* absl_nonnull,
                    google::protobuf::MessageFactory* absl_nonnull,
                    google::protobuf::Arena* absl_nonnull arena) {
  return string.Reverse(arena);
}

const Type& ListStringType() {
  static absl::NoDestructor<Type> kInstance(
      ListType(BuiltinsArena(), StringType()));
  return *kInstance;
}

absl::Status RegisterStringsDecls(TypeCheckerBuilder& builder) {
  // Runtime Supported functions.
  CEL_ASSIGN_OR_RETURN(
      auto join_decl,
      MakeFunctionDecl(
          "join",
          MakeMemberOverloadDecl("list_join", StringType(), ListStringType()),
          MakeMemberOverloadDecl("list_join_string", StringType(),
                                 ListStringType(), StringType())));
  CEL_ASSIGN_OR_RETURN(
      auto split_decl,
      MakeFunctionDecl(
          "split",
          MakeMemberOverloadDecl("string_split_string", ListStringType(),
                                 StringType(), StringType()),
          MakeMemberOverloadDecl("string_split_string_int", ListStringType(),
                                 StringType(), StringType(), IntType())));
  CEL_ASSIGN_OR_RETURN(
      auto lower_decl,
      MakeFunctionDecl("lowerAscii",
                       MakeMemberOverloadDecl("string_lower_ascii",
                                              StringType(), StringType())));

  CEL_ASSIGN_OR_RETURN(
      auto replace_decl,
      MakeFunctionDecl(
          "replace",
          MakeMemberOverloadDecl("string_replace_string_string", StringType(),
                                 StringType(), StringType(), StringType()),
          MakeMemberOverloadDecl("string_replace_string_string_int",
                                 StringType(), StringType(), StringType(),
                                 StringType(), IntType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(join_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(split_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(lower_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(replace_decl)));

  // Additional functions described in the spec.
  CEL_ASSIGN_OR_RETURN(
      auto char_at_decl,
      MakeFunctionDecl(
          "charAt", MakeMemberOverloadDecl("string_char_at_int", StringType(),
                                           StringType(), IntType())));
  CEL_ASSIGN_OR_RETURN(
      auto index_of_decl,
      MakeFunctionDecl(
          "indexOf",
          MakeMemberOverloadDecl("string_index_of_string", IntType(),
                                 StringType(), StringType()),
          MakeMemberOverloadDecl("string_index_of_string_int", IntType(),
                                 StringType(), StringType(), IntType())));
  CEL_ASSIGN_OR_RETURN(
      auto last_index_of_decl,
      MakeFunctionDecl(
          "lastIndexOf",
          MakeMemberOverloadDecl("string_last_index_of_string", IntType(),
                                 StringType(), StringType()),
          MakeMemberOverloadDecl("string_last_index_of_string_int", IntType(),
                                 StringType(), StringType(), IntType())));

  CEL_ASSIGN_OR_RETURN(
      auto substring_decl,
      MakeFunctionDecl(
          "substring",
          MakeMemberOverloadDecl("string_substring_int", StringType(),
                                 StringType(), IntType()),
          MakeMemberOverloadDecl("string_substring_int_int", StringType(),
                                 StringType(), IntType(), IntType())));
  CEL_ASSIGN_OR_RETURN(
      auto upper_ascii_decl,
      MakeFunctionDecl("upperAscii",
                       MakeMemberOverloadDecl("string_upper_ascii",
                                              StringType(), StringType())));
  CEL_ASSIGN_OR_RETURN(
      auto format_decl,
      MakeFunctionDecl("format",
                       MakeMemberOverloadDecl("string_format", StringType(),
                                              StringType(), ListType())));
  CEL_ASSIGN_OR_RETURN(
      auto quote_decl,
      MakeFunctionDecl(
          "strings.quote",
          MakeOverloadDecl("strings_quote", StringType(), StringType())));

  CEL_ASSIGN_OR_RETURN(
      auto reverse_decl,
      MakeFunctionDecl("reverse",
                       MakeMemberOverloadDecl("string_reverse", StringType(),
                                              StringType())));

  CEL_ASSIGN_OR_RETURN(
      auto trim_decl,
      MakeFunctionDecl("trim", MakeMemberOverloadDecl(
                                   "string_trim", StringType(), StringType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(char_at_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(index_of_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(last_index_of_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(substring_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(upper_ascii_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(format_decl)));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(quote_decl)));
  // MergeFunction is used to combine with the reverse function
  // defined in cel.lib.ext.lists extension.
  CEL_RETURN_IF_ERROR(builder.MergeFunction(std::move(reverse_decl)));
  CEL_RETURN_IF_ERROR(builder.MergeFunction(std::move(trim_decl)));

  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterStringsFunctions(FunctionRegistry& registry,
                                      const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          "join", /*receiver_style=*/true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          Join1)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, ListValue, StringValue>::
          CreateDescriptor("join", /*receiver_style=*/true),
      BinaryFunctionAdapter<absl::StatusOr<Value>, ListValue,
                            StringValue>::WrapFunction(Join2)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue>::
          CreateDescriptor("split", /*receiver_style=*/true),
      BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue,
                            StringValue>::WrapFunction(Split2)));
  CEL_RETURN_IF_ERROR(registry.Register(
      TernaryFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue,
          int64_t>::CreateDescriptor("split", /*receiver_style=*/true),
      TernaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue,
                             int64_t>::WrapFunction(Split3)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::
          CreateDescriptor("lowerAscii", /*receiver_style=*/true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::WrapFunction(
          LowerAscii)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::
          CreateDescriptor("upperAscii", /*receiver_style=*/true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::WrapFunction(
          UpperAscii)));
  CEL_RETURN_IF_ERROR(registry.Register(
      TernaryFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue,
          StringValue>::CreateDescriptor("replace", /*receiver_style=*/true),
      TernaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue,
                             StringValue>::WrapFunction(Replace1)));
  CEL_RETURN_IF_ERROR(registry.Register(
      QuaternaryFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue, StringValue,
          int64_t>::CreateDescriptor("replace", /*receiver_style=*/true),
      QuaternaryFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue,
                                StringValue, int64_t>::WrapFunction(Replace2)));
  CEL_RETURN_IF_ERROR(RegisterStringFormattingFunctions(registry, options));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, StringValue,
                             int64_t>::RegisterMemberOverload("charAt", &CharAt,
                                                              registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<int64_t, StringValue,
                             StringValue>::RegisterMemberOverload("indexOf",
                                                                  &IndexOf2,
                                                                  registry)));
  CEL_RETURN_IF_ERROR(
      (TernaryFunctionAdapter<Value, StringValue, StringValue,
                              int64_t>::RegisterMemberOverload("indexOf",
                                                               &IndexOf3,
                                                               registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<int64_t, StringValue,
                             StringValue>::RegisterMemberOverload("lastIndexOf",
                                                                  &LastIndexOf2,
                                                                  registry)));
  CEL_RETURN_IF_ERROR(
      (TernaryFunctionAdapter<Value, StringValue, StringValue,
                              int64_t>::RegisterMemberOverload("lastIndexOf",
                                                               &LastIndexOf3,
                                                               registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, StringValue,
                             int64_t>::RegisterMemberOverload("substring",
                                                              &Substring2,
                                                              registry)));
  CEL_RETURN_IF_ERROR(
      (TernaryFunctionAdapter<Value, StringValue, int64_t,
                              int64_t>::RegisterMemberOverload("substring",
                                                               &Substring3,
                                                               registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<StringValue, StringValue>::RegisterMemberOverload(
          "trim", &Trim, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<StringValue, StringValue>::RegisterGlobalOverload(
          "strings.quote", &Quote, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<StringValue, StringValue>::RegisterMemberOverload(
          "reverse", &Reverse, registry)));
  return absl::OkStatus();
}

absl::Status RegisterStringsFunctions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options) {
  return RegisterStringsFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

CheckerLibrary StringsCheckerLibrary() {
  return {"strings", &RegisterStringsDecls};
}

}  // namespace cel::extensions
