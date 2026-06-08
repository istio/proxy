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
#include <string>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "internal/minimal_descriptor_database.h"
#include "internal/minimal_descriptor_pool.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace cel::internal {

namespace {

ABSL_CONST_INIT const uint8_t kMinimalDescriptorSet[] = {
#include "internal/minimal_descriptor_set_embed.inc"
};

const google::protobuf::FileDescriptorSet* GetMinimumFileDescriptorSet() {
  static google::protobuf::FileDescriptorSet* const file_desc_set = []() {
    google::protobuf::FileDescriptorSet* file_desc_set = new google::protobuf::FileDescriptorSet();
    ABSL_CHECK(file_desc_set->ParseFromArray(  // Crash OK
       kMinimalDescriptorSet, ABSL_ARRAYSIZE(kMinimalDescriptorSet)));
    return file_desc_set;
  }();
  return file_desc_set;
}

}  // namespace

const google::protobuf::DescriptorPool* absl_nonnull GetMinimalDescriptorPool() {
  static const google::protobuf::DescriptorPool* absl_nonnull const pool = []() {
    const google::protobuf::FileDescriptorSet* file_desc_set =
        GetMinimumFileDescriptorSet();
    auto* pool = new google::protobuf::DescriptorPool();
    for (const auto& file_desc : file_desc_set->file()) {
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

namespace {

class DescriptorErrorCollector final
    : public google::protobuf::DescriptorPool::ErrorCollector {
 public:
  void RecordError(absl::string_view, absl::string_view element_name,
                   const google::protobuf::Message*, ErrorLocation,
                   absl::string_view message) override {
    errors_.push_back(absl::StrCat(element_name, ": ", message));
  }

  bool FoundErrors() const { return !errors_.empty(); }

  std::string FormatErrors() const { return absl::StrJoin(errors_, "\n\t"); }

 private:
  std::vector<std::string> errors_;
};

}  // namespace

absl::Status AddMinimumRequiredDescriptorsToPool(
    google::protobuf::DescriptorPool* absl_nonnull pool) {
  const google::protobuf::FileDescriptorSet* file_desc_set =
      GetMinimumFileDescriptorSet();
  for (const auto& file_desc : file_desc_set->file()) {
    if (pool->FindFileByName(file_desc.name()) != nullptr) {
      continue;
    }
    DescriptorErrorCollector error_collector;
    if (pool->BuildFileCollectingErrors(file_desc, &error_collector) ==
        nullptr) {
      ABSL_DCHECK(error_collector.FoundErrors());
      return absl::UnknownError(
          absl::StrCat("Failed to build file descriptor for ", file_desc.name(),
                       ":\n\t", error_collector.FormatErrors()));
    }
  }
  return absl::OkStatus();
}

}  // namespace cel::internal
