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

#include "runtime/runtime_builder_factory.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "internal/noop_delete.h"
#include "internal/status_macros.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"

namespace cel {

using ::cel::runtime_internal::RuntimeEnv;
using ::cel::runtime_internal::RuntimeImpl;

absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    const RuntimeOptions& options) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  return CreateRuntimeBuilder(
      std::shared_ptr<const google::protobuf::DescriptorPool>(
          descriptor_pool,
          internal::NoopDeleteFor<const google::protobuf::DescriptorPool>()),
      options);
}

absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder(
    absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool,
    const RuntimeOptions& options) {
  // TODO(uncreated-issue/57): and internal API for adding extensions that need to
  // downcast to the runtime impl.
  // TODO(uncreated-issue/56): add API for attaching an issue listener (replacing the
  // vector<status> overloads).
  ABSL_DCHECK(descriptor_pool != nullptr);
  auto environment = std::make_shared<RuntimeEnv>(std::move(descriptor_pool));
  CEL_RETURN_IF_ERROR(environment->Initialize());
  auto runtime_impl =
      std::make_unique<RuntimeImpl>(std::move(environment), options);
  runtime_impl->expr_builder().set_container(options.container);

  auto& type_registry = runtime_impl->type_registry();
  auto& function_registry = runtime_impl->function_registry();

  return RuntimeBuilder(type_registry, function_registry,
                        std::move(runtime_impl));
}

}  // namespace cel
