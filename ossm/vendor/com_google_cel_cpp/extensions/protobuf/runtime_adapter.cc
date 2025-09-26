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

#include "extensions/protobuf/runtime_adapter.h"

#include <memory>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/status/statusor.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/status_macros.h"
#include "runtime/runtime.h"

namespace cel::extensions {

absl::StatusOr<std::unique_ptr<TraceableProgram>>
ProtobufRuntimeAdapter::CreateProgram(
    const Runtime& runtime, const cel::expr::CheckedExpr& expr,
    const Runtime::CreateProgramOptions options) {
  CEL_ASSIGN_OR_RETURN(auto ast, CreateAstFromCheckedExpr(expr));
  return runtime.CreateTraceableProgram(std::move(ast), options);
}

absl::StatusOr<std::unique_ptr<TraceableProgram>>
ProtobufRuntimeAdapter::CreateProgram(
    const Runtime& runtime, const cel::expr::ParsedExpr& expr,
    const Runtime::CreateProgramOptions options) {
  CEL_ASSIGN_OR_RETURN(auto ast, CreateAstFromParsedExpr(expr));
  return runtime.CreateTraceableProgram(std::move(ast), options);
}

absl::StatusOr<std::unique_ptr<TraceableProgram>>
ProtobufRuntimeAdapter::CreateProgram(
    const Runtime& runtime, const cel::expr::Expr& expr,
    const cel::expr::SourceInfo* source_info,
    const Runtime::CreateProgramOptions options) {
  CEL_ASSIGN_OR_RETURN(auto ast, CreateAstFromParsedExpr(expr, source_info));
  return runtime.CreateTraceableProgram(std::move(ast), options);
}

}  // namespace cel::extensions
