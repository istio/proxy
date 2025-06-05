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

#include "internal/string_pool.h"

#include <cstring>  // IWYU pragma: keep
#include <string>   // IWYU pragma: keep

#include "absl/base/optimization.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/arena.h"

namespace cel::internal {

absl::string_view StringPool::InternString(absl::string_view string) {
  if (string.empty()) {
    return "";
  }
  return *strings_.lazy_emplace(string, [&](const auto& ctor) {
    ABSL_ASSUME(arena_ != nullptr);
    char* data = google::protobuf::Arena::CreateArray<char>(arena_, string.size());
    std::memcpy(data, string.data(), string.size());
    ctor(absl::string_view(data, string.size()));
  });
}

}  // namespace cel::internal
