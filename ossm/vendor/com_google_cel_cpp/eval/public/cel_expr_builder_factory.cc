/*
 * Copyright 2021 Google LLC
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

#include "eval/public/cel_expr_builder_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_options.h"
#include "eval/public/portable_cel_expr_builder_factory.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "internal/proto_util.h"

namespace google::api::expr::runtime {

namespace {
using ::google::api::expr::internal::ValidateStandardMessageTypes;
}  // namespace

std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory,
    const InterpreterOptions& options) {
  if (descriptor_pool == nullptr) {
    ABSL_LOG(ERROR) << "Cannot pass nullptr as descriptor pool to "
                       "CreateCelExpressionBuilder";
    return nullptr;
  }
  if (auto s = ValidateStandardMessageTypes(*descriptor_pool); !s.ok()) {
    ABSL_LOG(WARNING) << "Failed to validate standard message types: "
                      << s.ToString();  // NOLINT: OSS compatibility
    return nullptr;
  }

  auto builder =
      CreatePortableExprBuilder(std::make_unique<ProtobufDescriptorProvider>(
                                    descriptor_pool, message_factory),
                                options);
  return builder;
}

}  // namespace google::api::expr::runtime
