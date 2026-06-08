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

namespace cel::extensions {

namespace {

absl::StatusOr<Value> Base64Decode(
    const StringValue& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string in;
  std::string out;
  if (!absl::Base64Unescape(value.NativeString(in), &out)) {
    return ErrorValue{absl::InvalidArgumentError("invalid base64 data")};
  }
  return BytesValue(arena, std::move(out));
}

absl::StatusOr<Value> Base64Encode(
    const BytesValue& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string in;
  std::string out;
  absl::Base64Escape(value.NativeString(in), &out);
  return StringValue(arena, std::move(out));
}

absl::Status RegisterEncodersDecls(TypeCheckerBuilder& builder) {
  CEL_ASSIGN_OR_RETURN(
      auto base64_decode_decl,
      MakeFunctionDecl(
          "base64.decode",
          MakeOverloadDecl("base64_decode_string", BytesType(), StringType())));

  CEL_ASSIGN_OR_RETURN(
      auto base64_encode_decl,
      MakeFunctionDecl(
          "base64.encode",
          MakeOverloadDecl("base64_encode_bytes", StringType(), BytesType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(base64_decode_decl));
  CEL_RETURN_IF_ERROR(builder.AddFunction(base64_encode_decl));
  return absl::OkStatus();
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
    google::api::expr::runtime::CelFunctionRegistry* absl_nonnull registry,
    const google::api::expr::runtime::InterpreterOptions& options) {
  return RegisterEncodersFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

CheckerLibrary EncodersCheckerLibrary() {
  return {"cel.lib.ext.encoders", &RegisterEncodersDecls};
}

}  // namespace cel::extensions
