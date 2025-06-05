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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_POOL_H_

#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "common/arena_string.h"
#include "internal/string_pool.h"
#include "google/protobuf/arena.h"

namespace cel {

class ArenaStringPool;

absl::Nonnull<std::unique_ptr<ArenaStringPool>> NewArenaStringPool(
    absl::Nonnull<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

class ArenaStringPool final {
 public:
  ArenaStringPool(const ArenaStringPool&) = delete;
  ArenaStringPool(ArenaStringPool&&) = delete;
  ArenaStringPool& operator=(const ArenaStringPool&) = delete;
  ArenaStringPool& operator=(ArenaStringPool&&) = delete;

  ArenaString InternString(absl::string_view string) {
    return ArenaString(strings_.InternString(string));
  }

  ArenaString InternString(ArenaString) = delete;

 private:
  friend absl::Nonnull<std::unique_ptr<ArenaStringPool>> NewArenaStringPool(
      absl::Nonnull<google::protobuf::Arena*>);

  explicit ArenaStringPool(absl::Nonnull<google::protobuf::Arena*> arena)
      : strings_(arena) {}

  internal::StringPool strings_;
};

inline absl::Nonnull<std::unique_ptr<ArenaStringPool>> NewArenaStringPool(
    absl::Nonnull<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return std::unique_ptr<ArenaStringPool>(new ArenaStringPool(arena));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_POOL_H_
