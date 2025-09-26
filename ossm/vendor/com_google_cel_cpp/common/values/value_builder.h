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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_BUILDER_H_

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/value.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::common_internal {

// Like NewStructValueBuilder, but deals with well known types.
absl_nullable cel::ValueBuilderPtr NewValueBuilder(
    Allocator<> allocator,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    absl::string_view name);

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_BUILDER_H_
