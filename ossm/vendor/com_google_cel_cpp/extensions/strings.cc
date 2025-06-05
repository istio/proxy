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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "internal/utf8.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/internal/errors.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

struct AppendToStringVisitor {
  std::string& append_to;

  void operator()(absl::string_view string) const { append_to.append(string); }

  void operator()(const absl::Cord& cord) const {
    append_to.append(static_cast<std::string>(cord));
  }
};

absl::StatusOr<Value> Join2(ValueManager& value_manager, const ListValue& value,
                            const StringValue& separator) {
  std::string result;
  CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_manager));
  Value element;
  if (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, element));
    if (auto string_element = As<StringValue>(element); string_element) {
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
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, element));
    if (auto string_element = As<StringValue>(element); string_element) {
      string_element->NativeValue(AppendToStringVisitor{result});
    } else {
      return ErrorValue{
          runtime_internal::CreateNoMatchingOverloadError("join")};
    }
  }
  result.shrink_to_fit();
  // We assume the original string was well-formed.
  return value_manager.CreateUncheckedStringValue(std::move(result));
}

absl::StatusOr<Value> Join1(ValueManager& value_manager,
                            const ListValue& value) {
  return Join2(value_manager, value, StringValue{});
}

struct SplitWithEmptyDelimiter {
  ValueManager& value_manager;
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
      CEL_RETURN_IF_ERROR(builder.Add(
          value_manager.CreateUncheckedStringValue(absl::string_view(buffer))));
      --limit;
      string.remove_prefix(count);
    }
    if (!string.empty()) {
      CEL_RETURN_IF_ERROR(
          builder.Add(value_manager.CreateUncheckedStringValue(string)));
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
      CEL_RETURN_IF_ERROR(builder.Add(
          value_manager.CreateUncheckedStringValue(absl::string_view(buffer))));
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
      CEL_RETURN_IF_ERROR(builder.Add(
          value_manager.CreateUncheckedStringValue(std::move(buffer))));
    }
    return std::move(builder).Build();
  }
};

absl::StatusOr<Value> Split3(ValueManager& value_manager,
                             const StringValue& string,
                             const StringValue& delimiter, int64_t limit) {
  if (limit == 0) {
    // Per spec, when limit is 0 return an empty list.
    return ListValue{};
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat is as unlimited.
    limit = std::numeric_limits<int64_t>::max();
  }
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType{}));
  if (string.IsEmpty()) {
    // If string is empty, it doesn't matter what the delimiter is or the limit.
    // We just return a list with a single empty string.
    builder->Reserve(1);
    CEL_RETURN_IF_ERROR(builder->Add(StringValue{}));
    return std::move(*builder).Build();
  }
  if (delimiter.IsEmpty()) {
    // If the delimiter is empty, we split between every code point.
    return string.NativeValue(
        SplitWithEmptyDelimiter{value_manager, limit, *builder});
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
    CEL_RETURN_IF_ERROR(builder->Add(
        value_manager.CreateUncheckedStringValue(content_view.substr(0, pos))));
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
  CEL_RETURN_IF_ERROR(
      builder->Add(value_manager.CreateUncheckedStringValue(content_view)));
  return std::move(*builder).Build();
}

absl::StatusOr<Value> Split2(ValueManager& value_manager,
                             const StringValue& string,
                             const StringValue& delimiter) {
  return Split3(value_manager, string, delimiter, -1);
}

absl::StatusOr<Value> LowerAscii(ValueManager& value_manager,
                                 const StringValue& string) {
  std::string content = string.NativeString();
  absl::AsciiStrToLower(&content);
  // We assume the original string was well-formed.
  return value_manager.CreateUncheckedStringValue(std::move(content));
}

absl::StatusOr<Value> Replace2(ValueManager& value_manager,
                               const StringValue& string,
                               const StringValue& old_sub,
                               const StringValue& new_sub, int64_t limit) {
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

  return value_manager.CreateUncheckedStringValue(std::move(result));
}

absl::StatusOr<Value> Replace1(ValueManager& value_manager,
                               const StringValue& string,
                               const StringValue& old_sub,
                               const StringValue& new_sub) {
  return Replace2(value_manager, string, old_sub, new_sub, -1);
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
      VariadicFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue,
          int64_t>::CreateDescriptor("split", /*receiver_style=*/true),
      VariadicFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue,
                              int64_t>::WrapFunction(Split3)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::
          CreateDescriptor("lowerAscii", /*receiver_style=*/true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::WrapFunction(
          LowerAscii)));
  CEL_RETURN_IF_ERROR(registry.Register(
      VariadicFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue,
          StringValue>::CreateDescriptor("replace", /*receiver_style=*/true),
      VariadicFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue,
                              StringValue>::WrapFunction(Replace1)));
  CEL_RETURN_IF_ERROR(registry.Register(
      VariadicFunctionAdapter<
          absl::StatusOr<Value>, StringValue, StringValue, StringValue,
          int64_t>::CreateDescriptor("replace", /*receiver_style=*/true),
      VariadicFunctionAdapter<absl::StatusOr<Value>, StringValue, StringValue,
                              StringValue, int64_t>::WrapFunction(Replace2)));
  return absl::OkStatus();
}

absl::Status RegisterStringsFunctions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options) {
  return RegisterStringsFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
