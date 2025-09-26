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

#include "runtime/internal/runtime_env_testing.h"

#include <memory>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "internal/noop_delete.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/internal/runtime_env.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

absl_nonnull std::shared_ptr<RuntimeEnv> NewTestingRuntimeEnv() {
  auto env = std::make_shared<RuntimeEnv>(
      internal::GetSharedTestingDescriptorPool(),
      std::shared_ptr<google::protobuf::MessageFactory>(
          internal::GetTestingMessageFactory(),
          internal::NoopDeleteFor<google::protobuf::MessageFactory>()));
  ABSL_CHECK_OK(env->Initialize());  // Crash OK
  return env;
}

}  // namespace cel::runtime_internal
