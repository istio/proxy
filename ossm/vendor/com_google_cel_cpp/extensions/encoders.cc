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

#include "extensions/encoders.h"

#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

absl::StatusOr<Value> Base64Decode(ValueManager& value_manager,
                                   const StringValue& value) {
  std::string in;
  std::string out;
  if (!absl::Base64Unescape(value.NativeString(in), &out)) {
    return ErrorValue{absl::InvalidArgumentError("invalid base64 data")};
  }
  return value_manager.CreateBytesValue(std::move(out));
}

absl::StatusOr<Value> Base64Encode(ValueManager& value_manager,
                                   const BytesValue& value) {
  std::string in;
  std::string out;
  absl::Base64Escape(value.NativeString(in), &out);
  return value_manager.CreateStringValue(std::move(out));
}

}  // namespace

absl::Status RegisterEncodersFunctions(FunctionRegistry& registry,
                                       const RuntimeOptions&) {
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           StringValue>::CreateDescriptor("base64.decode",
                                                          false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, StringValue>::WrapFunction(
          &Base64Decode)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, BytesValue>::CreateDescriptor(
          "base64.encode", false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, BytesValue>::WrapFunction(
          &Base64Encode)));
  return absl::OkStatus();
}

absl::Status RegisterEncodersFunctions(
    absl::Nonnull<google::api::expr::runtime::CelFunctionRegistry*> registry,
    const google::api::expr::runtime::InterpreterOptions& options) {
  return RegisterEncodersFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
