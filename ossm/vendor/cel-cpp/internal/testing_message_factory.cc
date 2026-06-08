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

#include "internal/testing_message_factory.h"

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"

namespace cel::internal {

google::protobuf::MessageFactory* absl_nonnull GetTestingMessageFactory() {
  static absl::NoDestructor<google::protobuf::DynamicMessageFactory> factory(
      GetTestingDescriptorPool());
  return &*factory;
}

}  // namespace cel::internal
