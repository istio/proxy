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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_RUNTIME_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_RUNTIME_ADAPTER_H_

#include <memory>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

// Helper class for cel::Runtime that converts the pb serialization format for
// expressions to the internal AST format.
class ProtobufRuntimeAdapter {
 public:
  // Only to be used for static member functions.
  ProtobufRuntimeAdapter() = delete;

  static absl::StatusOr<std::unique_ptr<TraceableProgram>> CreateProgram(
      const Runtime& runtime, const google::api::expr::v1alpha1::CheckedExpr& expr,
      const Runtime::CreateProgramOptions options = {});
  static absl::StatusOr<std::unique_ptr<TraceableProgram>> CreateProgram(
      const Runtime& runtime, const google::api::expr::v1alpha1::ParsedExpr& expr,
      const Runtime::CreateProgramOptions options = {});
  static absl::StatusOr<std::unique_ptr<TraceableProgram>> CreateProgram(
      const Runtime& runtime, const google::api::expr::v1alpha1::Expr& expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info = nullptr,
      const Runtime::CreateProgramOptions options = {});
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_RUNTIME_ADAPTER_H_
