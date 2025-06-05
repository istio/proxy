/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_PORTABLE_CEL_EXPR_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_PORTABLE_CEL_EXPR_BUILDER_FACTORY_H_

#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/structs/legacy_type_provider.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory for initializing a CelExpressionBuilder implementation for public
// use.
//
// This version does not include any message type information, instead deferring
// to the type_provider argument. type_provider is guaranteed to be the first
// type provider in the type registry.
std::unique_ptr<CelExpressionBuilder> CreatePortableExprBuilder(
    std::unique_ptr<LegacyTypeProvider> type_provider,
    const InterpreterOptions& options = InterpreterOptions());

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_PORTABLE_CEL_EXPR_BUILDER_FACTORY_H_
