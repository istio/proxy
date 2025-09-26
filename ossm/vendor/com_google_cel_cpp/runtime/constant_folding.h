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

#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "runtime/runtime_builder.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

// Enable constant folding in the runtime being built.
//
// Constant folding eagerly evaluates sub-expressions with all constant inputs
// at plan time to simplify the resulting program. User functions are executed
// if they are eagerly bound.
//
// The provided, the `google::protobuf::Arena` must outlive the resulting runtime
// and any program it creates. Otherwise the runtime will create one as needed
// during planning for each program, unless one is explicitly provided during
// planning.
//
// The provided, the `google::protobuf::MessageFactory` must outlive the resulting runtime
// and any program it creates. Otherwise the runtime will create one as needed
// and use it for all planning and the resulting programs created from the
// runtime, unless one is explicitly provided during planning or evaluation.
absl::Status EnableConstantFolding(RuntimeBuilder& builder);
absl::Status EnableConstantFolding(RuntimeBuilder& builder,
                                   google::protobuf::Arena* absl_nonnull arena);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, absl_nonnull std::shared_ptr<google::protobuf::Arena> arena);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder,
    google::protobuf::MessageFactory* absl_nonnull message_factory);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder,
    absl_nonnull std::shared_ptr<google::protobuf::MessageFactory> message_factory);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, google::protobuf::Arena* absl_nonnull arena,
    google::protobuf::MessageFactory* absl_nonnull message_factory);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, google::protobuf::Arena* absl_nonnull arena,
    absl_nonnull std::shared_ptr<google::protobuf::MessageFactory> message_factory);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, absl_nonnull std::shared_ptr<google::protobuf::Arena> arena,
    google::protobuf::MessageFactory* absl_nonnull message_factory);
absl::Status EnableConstantFolding(
    RuntimeBuilder& builder, absl_nonnull std::shared_ptr<google::protobuf::Arena> arena,
    absl_nonnull std::shared_ptr<google::protobuf::MessageFactory> message_factory);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_CONSTANT_FOLDING_H_
