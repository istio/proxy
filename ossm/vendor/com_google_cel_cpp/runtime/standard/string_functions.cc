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

#include "runtime/standard/string_functions.h"

#include <cstdint>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

// Concatenation for string type.
absl::StatusOr<StringValue> ConcatString(
    const StringValue& value1, const StringValue& value2,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull arena) {
  return StringValue::Concat(value1, value2, arena);
}

// Concatenation for bytes type.
absl::StatusOr<BytesValue> ConcatBytes(
    const BytesValue& value1, const BytesValue& value2,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull arena) {
  return BytesValue::Concat(value1, value2, arena);
}

bool StringContains(const StringValue& value, const StringValue& substr) {
  return value.Contains(substr);
}

bool StringEndsWith(const StringValue& value, const StringValue& suffix) {
  return value.EndsWith(suffix);
}

bool StringStartsWith(const StringValue& value, const StringValue& prefix) {
  return value.StartsWith(prefix);
}

absl::Status RegisterSizeFunctions(FunctionRegistry& registry) {
  // String size
  auto size_func = [](const StringValue& value) -> int64_t {
    return value.Size();
  };

  // Support global and receiver style size() operations on strings.
  using StrSizeFnAdapter = UnaryFunctionAdapter<int64_t, const StringValue&>;
  CEL_RETURN_IF_ERROR(StrSizeFnAdapter::RegisterGlobalOverload(
      cel::builtin::kSize, size_func, registry));

  CEL_RETURN_IF_ERROR(StrSizeFnAdapter::RegisterMemberOverload(
      cel::builtin::kSize, size_func, registry));

  // Bytes size
  auto bytes_size_func = [](const BytesValue& value) -> int64_t {
    return value.Size();
  };

  // Support global and receiver style size() operations on bytes.
  using BytesSizeFnAdapter = UnaryFunctionAdapter<int64_t, const BytesValue&>;
  CEL_RETURN_IF_ERROR(BytesSizeFnAdapter::RegisterGlobalOverload(
      cel::builtin::kSize, bytes_size_func, registry));

  return BytesSizeFnAdapter::RegisterMemberOverload(cel::builtin::kSize,
                                                    bytes_size_func, registry);
}

absl::Status RegisterConcatFunctions(FunctionRegistry& registry) {
  using StrCatFnAdapter =
      BinaryFunctionAdapter<absl::StatusOr<StringValue>, const StringValue&,
                            const StringValue&>;
  CEL_RETURN_IF_ERROR(StrCatFnAdapter::RegisterGlobalOverload(
      cel::builtin::kAdd, &ConcatString, registry));

  using BytesCatFnAdapter =
      BinaryFunctionAdapter<absl::StatusOr<BytesValue>, const BytesValue&,
                            const BytesValue&>;
  return BytesCatFnAdapter::RegisterGlobalOverload(cel::builtin::kAdd,
                                                   &ConcatBytes, registry);
}

}  // namespace

absl::Status RegisterStringFunctions(FunctionRegistry& registry,
                                     const RuntimeOptions& options) {
  // Basic substring tests (contains, startsWith, endsWith)
  for (bool receiver_style : {true, false}) {
    auto status =
        BinaryFunctionAdapter<bool, const StringValue&, const StringValue&>::
            Register(cel::builtin::kStringContains, receiver_style,
                     StringContains, registry);
    CEL_RETURN_IF_ERROR(status);

    status =
        BinaryFunctionAdapter<bool, const StringValue&, const StringValue&>::
            Register(cel::builtin::kStringEndsWith, receiver_style,
                     StringEndsWith, registry);
    CEL_RETURN_IF_ERROR(status);

    status =
        BinaryFunctionAdapter<bool, const StringValue&, const StringValue&>::
            Register(cel::builtin::kStringStartsWith, receiver_style,
                     StringStartsWith, registry);
    CEL_RETURN_IF_ERROR(status);
  }

  // string concatenation if enabled
  if (options.enable_string_concat) {
    CEL_RETURN_IF_ERROR(RegisterConcatFunctions(registry));
  }

  return RegisterSizeFunctions(registry);
}

}  // namespace cel
