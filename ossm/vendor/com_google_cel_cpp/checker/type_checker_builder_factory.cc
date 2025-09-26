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

#include "checker/type_checker_builder_factory.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "checker/checker_options.h"
#include "checker/internal/type_checker_builder_impl.h"
#include "checker/type_checker_builder.h"
#include "internal/noop_delete.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/descriptor.h"

namespace cel {

absl::StatusOr<std::unique_ptr<TypeCheckerBuilder>> CreateTypeCheckerBuilder(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    const CheckerOptions& options) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  return CreateTypeCheckerBuilder(
      std::shared_ptr<const google::protobuf::DescriptorPool>(
          descriptor_pool,
          internal::NoopDeleteFor<const google::protobuf::DescriptorPool>()),
      options);
}

absl::StatusOr<std::unique_ptr<TypeCheckerBuilder>> CreateTypeCheckerBuilder(
    absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool,
    const CheckerOptions& options) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  // Verify the standard descriptors, we do not need to keep
  // `well_known_types::Reflection` at the moment here.
  CEL_RETURN_IF_ERROR(
      well_known_types::Reflection().Initialize(descriptor_pool.get()));
  return std::make_unique<checker_internal::TypeCheckerBuilderImpl>(
      std::move(descriptor_pool), options);
}

}  // namespace cel
