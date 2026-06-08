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
//
// Definitions for instrumenting a CEL expression at the planner level.
//
// CEL users should not use this directly.
#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_INSTRUMENTATION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_INSTRUMENTATION_H_

#include <cstdint>
#include <functional>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "common/ast.h"
#include "common/value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"

namespace google::api::expr::runtime {

// Instrumentation inspects intermediate values after the evaluation of an
// expression node.
//
// Unlike traceable expressions, this callback is applied across all
// evaluations of an expression. Implementations must be thread safe if the
// expression is evaluated concurrently.
using Instrumentation =
    std::function<absl::Status(int64_t expr_id, const cel::Value&)>;

// A factory for creating Instrumentation instances.
//
// This allows the extension implementations to map from a given ast to a
// specific instrumentation instance.
//
// An empty function object may be returned to skip instrumenting the given
// expression.
using InstrumentationFactory =
    absl::AnyInvocable<Instrumentation(const cel::Ast&) const>;

// Create a new Instrumentation extension.
//
// These should typically be added last if any program optimizations are
// applied.
ProgramOptimizerFactory CreateInstrumentationExtension(
    InstrumentationFactory factory);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_INSTRUMENTATION_H_
