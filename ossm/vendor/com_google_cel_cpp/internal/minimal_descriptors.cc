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

#include <cstdint>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "internal/minimal_descriptor_database.h"
#include "internal/minimal_descriptor_pool.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace cel::internal {

namespace {

ABSL_CONST_INIT const uint8_t kMinimalDescriptorSet[] = {
#include "internal/minimal_descriptor_set_embed.inc"
};

}  // namespace

const google::protobuf::DescriptorPool* absl_nonnull GetMinimalDescriptorPool() {
  static const google::protobuf::DescriptorPool* absl_nonnull const pool = []() {
    google::protobuf::FileDescriptorSet file_desc_set;
    ABSL_CHECK(file_desc_set.ParseFromArray(  // Crash OK
       kMinimalDescriptorSet, ABSL_ARRAYSIZE(kMinimalDescriptorSet)));
    auto* pool = new google::protobuf::DescriptorPool();
    for (const auto& file_desc : file_desc_set.file()) {
      ABSL_CHECK(pool->BuildFile(file_desc) != nullptr);  // Crash OK
    }
    return pool;
  }();
  return pool;
}

google::protobuf::DescriptorDatabase* absl_nonnull GetMinimalDescriptorDatabase() {
  static absl::NoDestructor<google::protobuf::DescriptorPoolDatabase> database(
      *GetMinimalDescriptorPool());
  return &*database;
}

}  // namespace cel::internal
