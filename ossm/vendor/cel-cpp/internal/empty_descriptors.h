// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_EMPTY_DESCRIPTORS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_EMPTY_DESCRIPTORS_H_

#include "absl/base/nullability.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::internal {

// GetEmptyDefaultInstance returns a pointer to a `google::protobuf::Message` which is an
// instance of `google.protobuf.Empty`. The returned `google::protobuf::Message` is valid
// for the lifetime of the process.
const google::protobuf::Message* absl_nonnull GetEmptyDefaultInstance();

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_EMPTY_DESCRIPTORS_H_
