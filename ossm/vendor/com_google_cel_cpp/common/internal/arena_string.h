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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_ARENA_STRING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_ARENA_STRING_H_

#include "absl/strings/string_view.h"

namespace cel::common_internal {

// `ArenaString` is effectively `absl::string_view` but as a separate distinct
// type. It is used to indicate that the underlying storage of the string is
// owned by an arena or pooling memory manager.
class ArenaString final {
 public:
  ArenaString() = default;
  ArenaString(const ArenaString&) = default;
  ArenaString& operator=(const ArenaString&) = default;

  explicit ArenaString(absl::string_view content) : content_(content) {}

  typename absl::string_view::size_type size() const { return content_.size(); }

  typename absl::string_view::const_pointer data() const {
    return content_.data();
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::string_view() const { return content_; }

 private:
  absl::string_view content_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_ARENA_STRING_H_
