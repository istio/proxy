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

#include <cstring>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/arena.h"

namespace cel::internal {

absl::string_view StringPool::InternString(absl::string_view string) {
  if (string.empty()) {
    return "";
  }
  return *strings_.lazy_emplace(string, [&](const auto& ctor) {
    char* data =
        reinterpret_cast<char*>(arena()->AllocateAligned(string.size()));
    std::memcpy(data, string.data(), string.size());
    ctor(absl::string_view(data, string.size()));
  });
}

absl::string_view StringPool::InternString(std::string&& string) {
  if (string.empty()) {
    return "";
  }
  return *strings_.lazy_emplace(string, [&](const auto& ctor) {
    if (string.size() <= sizeof(std::string)) {
      char* data =
          reinterpret_cast<char*>(arena()->AllocateAligned(string.size()));
      std::memcpy(data, string.data(), string.size());
      ctor(absl::string_view(data, string.size()));
    } else {
      google::protobuf::Arena* arena = this->arena();
      ABSL_ASSUME(arena != nullptr);
      ctor(absl::string_view(
          *google::protobuf::Arena::Create<std::string>(arena, std::move(string))));
    }
  });
}

absl::string_view StringPool::InternString(const absl::Cord& string) {
  if (string.empty()) {
    return "";
  }
  return *strings_.lazy_emplace(string, [&](const auto& ctor) {
    char* data =
        reinterpret_cast<char*>(arena()->AllocateAligned(string.size()));
    absl::Cord::CharIterator string_begin = string.char_begin();
    const absl::Cord::CharIterator string_end = string.char_end();
    char* p = data;
    while (string_begin != string_end) {
      absl::string_view chunk = absl::Cord::ChunkRemaining(string_begin);
      std::memcpy(p, chunk.data(), chunk.size());
      p += chunk.size();
      absl::Cord::Advance(&string_begin, chunk.size());
    }
    ctor(absl::string_view(data, string.size()));
  });
}

}  // namespace cel::internal
