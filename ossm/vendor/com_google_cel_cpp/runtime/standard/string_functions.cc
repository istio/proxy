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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"

namespace cel {
namespace {

// Concatenation for string type.
absl::StatusOr<StringValue> ConcatString(ValueManager& factory,
                                         const StringValue& value1,
                                         const StringValue& value2) {
  // TODO: use StringValue::Concat when remaining interop usages
  // removed. Modern concat implementation forces additional copies when
  // converting to legacy string values.
  return factory.CreateUncheckedStringValue(
      absl::StrCat(value1.ToString(), value2.ToString()));
}

// Concatenation for bytes type.
absl::StatusOr<BytesValue> ConcatBytes(ValueManager& factory,
                                       const BytesValue& value1,
                                       const BytesValue& value2) {
  // TODO: use BytesValue::Concat when remaining interop usages
  // removed. Modern concat implementation forces additional copies when
  // converting to legacy string values.
  return factory.CreateBytesValue(
      absl::StrCat(value1.ToString(), value2.ToString()));
}

bool StringContains(ValueManager&, const StringValue& value,
                    const StringValue& substr) {
  return absl::StrContains(value.ToString(), substr.ToString());
}

bool StringEndsWith(ValueManager&, const StringValue& value,
                    const StringValue& suffix) {
  return absl::EndsWith(value.ToString(), suffix.ToString());
}

bool StringStartsWith(ValueManager&, const StringValue& value,
                      const StringValue& prefix) {
  return absl::StartsWith(value.ToString(), prefix.ToString());
}

absl::Status RegisterSizeFunctions(FunctionRegistry& registry) {
  // String size
  auto size_func = [](ValueManager& value_factory,
                      const StringValue& value) -> int64_t {
    return value.Size();
  };

  // Support global and receiver style size() operations on strings.
  using StrSizeFnAdapter = UnaryFunctionAdapter<int64_t, const StringValue&>;
  CEL_RETURN_IF_ERROR(StrSizeFnAdapter::RegisterGlobalOverload(
      cel::builtin::kSize, size_func, registry));

  CEL_RETURN_IF_ERROR(StrSizeFnAdapter::RegisterMemberOverload(
      cel::builtin::kSize, size_func, registry));

  // Bytes size
  auto bytes_size_func = [](ValueManager&, const BytesValue& value) -> int64_t {
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
