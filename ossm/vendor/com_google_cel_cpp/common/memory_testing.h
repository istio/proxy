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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_MEMORY_TESTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_MEMORY_TESTING_H_

#include <string>
#include <tuple>

#include "absl/strings/str_join.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

template <typename... Ts>
class ThreadCompatibleMemoryTest
    : public ::testing::TestWithParam<std::tuple<MemoryManagement, Ts...>> {
 public:
  void SetUp() override {}

  void TearDown() override { Finish(); }

  MemoryManagement memory_management() { return std::get<0>(this->GetParam()); }

  MemoryManagerRef memory_manager() {
    switch (memory_management()) {
      case MemoryManagement::kReferenceCounting:
        return MemoryManager::ReferenceCounting();
        break;
      case MemoryManagement::kPooling:
        if (!arena_) {
          arena_.emplace();
        }
        return MemoryManager::Pooling(&*arena_);
        break;
    }
  }

  void Finish() { arena_.reset(); }

  static std::string ToString(
      ::testing::TestParamInfo<std::tuple<MemoryManagement, Ts...>> param) {
    return absl::StrJoin(param.param, "_", absl::StreamFormatter());
  }

 protected:
  virtual MemoryManager NewThreadCompatiblePoolingMemoryManager() {
    return MemoryManager::Pooling(&*arena_);
  }

 private:
  absl::optional<google::protobuf::Arena> arena_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_MEMORY_TESTING_H_
