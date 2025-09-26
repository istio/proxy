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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_VALUE_H_

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;

namespace common_internal {

// Adapts the given instance of the well known message type
// `google.protobuf.Value` to `cel::Value`. If the underlying value is a string
// and the string had to be copied, `allocator` will be used to create a new
// string value. This should be rare and unlikely.
Value ParsedJsonValue(const google::protobuf::Message* absl_nonnull message,
                      google::protobuf::Arena* absl_nonnull arena);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_VALUE_H_
