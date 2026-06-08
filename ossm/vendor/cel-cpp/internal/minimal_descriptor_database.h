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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_MINIMAL_DESCRIPTOR_DATABASE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_MINIMAL_DESCRIPTOR_DATABASE_H_

#include "absl/base/nullability.h"
#include "google/protobuf/descriptor_database.h"

namespace cel::internal {

// GetMinimalDescriptorDatabase returns a pointer to a
// `google::protobuf::DescriptorDatabase` which includes has the minimally necessary
// descriptors required by the Common Expression Language. The returning
// `proto2::DescripDescriptorDatabasetorPool` is valid for the lifetime of the
// process.
google::protobuf::DescriptorDatabase* absl_nonnull GetMinimalDescriptorDatabase();

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MINIMAL_DESCRIPTOR_DATABASE_H_
