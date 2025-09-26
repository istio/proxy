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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
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
#include "internal/utf8.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/internal/errors.h"
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
  std::string result;
  CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator());
  Value element;
  if (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(
        iterator->Next(descriptor_pool, message_factory, arena, &element));
    if (auto string_element = element.AsString(); string_element) {
      string_element->NativeValue(AppendToStringVisitor{result});
    } else {
      return ErrorValue{
          runtime_internal::CreateNoMatchingOverloadError("join")};
    }
  }
  std::string separator_scratch;
  absl::string_view separator_view = separator.NativeString(separator_scratch);
  while (iterator->HasNext()) {
    result.append(separator_view);
    CEL_RETURN_IF_ERROR(
        iterator->Next(descriptor_pool, message_factory, arena, &element));
    if (auto string_element = element.AsString(); string_element) {
      string_element->NativeValue(AppendToStringVisitor{result});
    } else {
      return ErrorValue{
          runtime_internal::CreateNoMatchingOverloadError("join")};
    }
  }
  result.shrink_to_fit();
  // We assume the original string was well-formed.
  return StringValue(arena, std::move(result));
}

absl::StatusOr<Value> Join1(
    const ListValue& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return Join2(value, StringValue{}, descriptor_pool, message_factory, arena);
}

struct SplitWithEmptyDelimiter {
  google::protobuf::Arena* absl_nonnull arena;
  int64_t& limit;
  ListValueBuilder& builder;

  absl::StatusOr<Value> operator()(absl::string_view string) const {
    char32_t rune;
    size_t count;
    std::string buffer;
    buffer.reserve(4);
    while (!string.empty() && limit > 1) {
      std::tie(rune, count) = internal::Utf8Decode(string);
      buffer.clear();
      internal::Utf8Encode(buffer, rune);
      CEL_RETURN_IF_ERROR(
          builder.Add(StringValue(arena, absl::string_view(buffer))));
      --limit;
      string.remove_prefix(count);
    }
    if (!string.empty()) {
      CEL_RETURN_IF_ERROR(builder.Add(StringValue(arena, string)));
    }
    return std::move(builder).Build();
  }

  absl::StatusOr<Value> operator()(const absl::Cord& string) const {
    auto begin = string.char_begin();
    auto end = string.char_end();
    char32_t rune;
    size_t count;
    std::string buffer;
    while (begin != end && limit > 1) {
      std::tie(rune, count) = internal::Utf8Decode(begin);
      buffer.clear();
      internal::Utf8Encode(buffer, rune);
      CEL_RETURN_IF_ERROR(
          builder.Add(StringValue(arena, absl::string_view(buffer))));
      --limit;
      absl::Cord::Advance(&begin, count);
    }
    if (begin != end) {
      buffer.clear();
      while (begin != end) {
        auto chunk = absl::Cord::ChunkRemaining(begin);
        buffer.append(chunk);
        absl::Cord::Advance(&begin, chunk.size());
      }
      buffer.shrink_to_fit();
      CEL_RETURN_IF_ERROR(builder.Add(StringValue(arena, std::move(buffer))));
    }
    return std::move(builder).Build();
  }
};

absl::StatusOr<Value> Split3(
    const StringValue& string, const StringValue& delimiter, int64_t limit,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (limit == 0) {
    // Per spec, when limit is 0 return an empty list.
    return ListValue{};
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat is as unlimited.
    limit = std::numeric_limits<int64_t>::max();
  }
  auto builder = NewListValueBuilder(arena);
  if (string.IsEmpty()) {
    // If string is empty, it doesn't matter what the delimiter is or the limit.
    // We just return a list with a single empty string.
    builder->Reserve(1);
    CEL_RETURN_IF_ERROR(builder->Add(StringValue{}));
    return std::move(*builder).Build();
  }
  if (delimiter.IsEmpty()) {
    // If the delimiter is empty, we split between every code point.
    return string.NativeValue(SplitWithEmptyDelimiter{arena, limit, *builder});
  }
  // At this point we know the string is not empty and the delimiter is not
  // empty.
  std::string delimiter_scratch;
  absl::string_view delimiter_view = delimiter.NativeString(delimiter_scratch);
  std::string content_scratch;
  absl::string_view content_view = string.NativeString(content_scratch);
  while (limit > 1 && !content_view.empty()) {
    auto pos = content_view.find(delimiter_view);
    if (pos == absl::string_view::npos) {
      break;
    }
    // We assume the original string was well-formed.
    CEL_RETURN_IF_ERROR(
        builder->Add(StringValue(arena, content_view.substr(0, pos))));
    --limit;
    content_view.remove_prefix(pos + delimiter_view.size());
    if (content_view.empty()) {
      // We found the delimiter at the end of the string. Add an empty string
      // to the end of the list.
      CEL_RETURN_IF_ERROR(builder->Add(StringValue{}));
      return std::move(*builder).Build();
    }
  }
  // We have one left in the limit or do not have any more matches. Add
  // whatever is left as the remaining entry.
  //
  // We assume the original string was well-formed.
  CEL_RETURN_IF_ERROR(builder->Add(StringValue(arena, content_view)));
  return std::move(*builder).Build();
}

absl::StatusOr<Value> Split2(
    const StringValue& string, const StringValue& delimiter,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return Split3(string, delimiter, -1, descriptor_pool, message_factory, arena);
}

absl::StatusOr<Value> LowerAscii(const StringValue& string,
                                 const google::protobuf::DescriptorPool* absl_nonnull,
                                 google::protobuf::MessageFactory* absl_nonnull,
                                 google::protobuf::Arena* absl_nonnull arena) {
  std::string content = string.NativeString();
  absl::AsciiStrToLower(&content);
  // We assume the original string was well-formed.
  return StringValue(arena, std::move(content));
}

absl::StatusOr<Value> UpperAscii(const StringValue& string,
                                 const google::protobuf::DescriptorPool* absl_nonnull,
                                 google::protobuf::MessageFactory* absl_nonnull,
                                 google::protobuf::Arena* absl_nonnull arena) {
  std::string content = string.NativeString();
  absl::AsciiStrToUpper(&content);
  // We assume the original string was well-formed.
  return StringValue(arena, std::move(content));
}

absl::StatusOr<Value> Replace2(const StringValue& string,
                               const StringValue& old_sub,
                               const StringValue& new_sub, int64_t limit,
                               const google::protobuf::DescriptorPool* absl_nonnull,
                               google::protobuf::MessageFactory* absl_nonnull,
                               google::protobuf::Arena* absl_nonnull arena) {
  if (limit == 0) {
    // When the replacement limit is 0, the result is the original string.
    return string;
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat is as unlimited.
    limit = std::numeric_limits<int64_t>::max();
  }

  std::string result;
  std::string old_sub_scratch;
  absl::string_view old_sub_view = old_sub.NativeString(old_sub_scratch);
  std::string new_sub_scratch;
  absl::string_view new_sub_view = new_sub.NativeString(new_sub_scratch);
  std::string content_scratch;
  absl::string_view content_view = string.NativeString(content_scratch);
  while (limit > 0 && !content_view.empty()) {
    auto pos = content_view.find(old_sub_view);
    if (pos == absl::string_view::npos) {
      break;
    }
    result.append(content_view.substr(0, pos));
    result.append(new_sub_view);
    --limit;
    content_view.remove_prefix(pos + old_sub_view.size());
  }
  // Add the remainder of the string.
  if (!content_view.empty()) {
    result.append(content_view);
  }

  return StringValue(arena, std::move(result));
}

absl::StatusOr<Value> Replace1(
    const StringValue& string, const StringValue& old_sub,
    const StringValue& new_sub,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  return Replace2(string, old_sub, new_sub, -1, descriptor_pool,
                  message_factory, arena);
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
