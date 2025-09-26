// Copyright 2019 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_

#include <memory>

#include "absl/base/nullability.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

// Create a new constant folding extension.
// Eagerly evaluates sub expressions with all constant inputs, and replaces said
// sub expression with the result.
//
// Note: the precomputed values may be allocated using the provided
// MemoryManager so it must outlive any programs created with this
// extension.
google::api::expr::runtime::ProgramOptimizerFactory
CreateConstantFoldingOptimizer(
    absl_nullable std::shared_ptr<google::protobuf::Arena> arena = nullptr,
    absl_nullable std::shared_ptr<google::protobuf::MessageFactory> message_factory =
        nullptr);

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CONSTANT_FOLDING_H_
