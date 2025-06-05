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

#include "common/internal/shared_byte_string.h"

#include <cstdint>
#include <string>

#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/internal/arena_string.h"
#include "common/internal/reference_count.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

SharedByteString::SharedByteString(Allocator<> allocator,
                                   absl::string_view value)
    : header_(/*is_cord=*/false, /*size=*/value.size()) {
  if (value.empty()) {
    content_.string.data = "";
    content_.string.refcount = 0;
  } else {
    if (auto* arena = allocator.arena(); arena != nullptr) {
      content_.string.data =
          google::protobuf::Arena::Create<std::string>(arena, value)->data();
      content_.string.refcount = 0;
      return;
    }
    auto pair = MakeReferenceCountedString(value);
    content_.string.data = pair.second.data();
    content_.string.refcount = reinterpret_cast<uintptr_t>(pair.first);
  }
}

SharedByteString::SharedByteString(Allocator<> allocator,
                                   const absl::Cord& value)
    : header_(/*is_cord=*/allocator.arena() == nullptr,
              /*size=*/allocator.arena() == nullptr ? 0 : value.size()) {
  if (header_.is_cord) {
    ::new (static_cast<void*>(cord_ptr())) absl::Cord(value);
  } else {
    if (value.empty()) {
      content_.string.data = "";
    } else {
      auto* string = google::protobuf::Arena::Create<std::string>(allocator.arena());
      absl::CopyCordToString(value, string);
      content_.string.data = string->data();
    }
    content_.string.refcount = 0;
  }
}

SharedByteString SharedByteString::Clone(Allocator<> allocator) const {
  if (absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      arena != nullptr) {
    if (!header_.is_cord && (IsPooledString() || !IsManagedString())) {
      return *this;
    }
    auto* cloned = google::protobuf::Arena::Create<std::string>(arena);
    Visit(absl::Overload(
        [cloned](absl::string_view string) {
          cloned->assign(string.data(), string.size());
        },
        [cloned](const absl::Cord& cord) {
          absl::CopyCordToString(cord, cloned);
        }));
    return SharedByteString(ArenaString(*cloned));
  }
  return *this;
}

}  // namespace cel::common_internal
