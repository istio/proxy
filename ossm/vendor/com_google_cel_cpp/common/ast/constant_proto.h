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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_CONSTANT_PROTO_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_CONSTANT_PROTO_H_

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/constant.h"

namespace cel::ast_internal {

// `ConstantToProto` converts from native `Constant` to its protocol buffer
// message equivalent.
absl::Status ConstantToProto(const Constant& constant,
                             cel::expr::Constant* absl_nonnull proto);

// `ConstantToProto` converts to native `Constant` from its protocol buffer
// message equivalent.
absl::Status ConstantFromProto(const cel::expr::Constant& proto,
                               Constant& constant);

}  // namespace cel::ast_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_CONSTANT_PROTO_H_
