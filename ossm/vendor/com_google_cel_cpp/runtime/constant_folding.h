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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_CONSTANT_FOLDING_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_CONSTANT_FOLDING_H_

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/allocator.h"
#include "runtime/runtime_builder.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

// Enable constant folding in the runtime being built.
//
// Constant folding eagerly evaluates sub-expressions with all constant inputs
// at plan time to simplify the resulting program. User extensions functions are
// executed if they are eagerly bound.
//
// The underlying implementation of `allocator` must outlive the resulting
// runtime and any programs it creates.
//
// The provided `google::protobuf::MessageFactory` must outlive the resulting runtime and
// any program it creates. Failure to pass a message factory may result in
// certain optimizations being disabled.
absl::Status EnableConstantFolding(RuntimeBuilder& builder,
                                   Allocator<> allocator);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, Allocator<> allocator,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_CONSTANT_FOLDING_H_
