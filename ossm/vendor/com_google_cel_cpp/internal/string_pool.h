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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_STRING_POOL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_STRING_POOL_H_

#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/arena.h"

namespace cel::internal {

// `StringPool` efficiently performs string interning using `google::protobuf::Arena`.
//
// This class is thread compatible, but typically requires external
// synchronization or serial usage.
class StringPool final {
 public:
  explicit StringPool(
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : arena_(ABSL_DIE_IF_NULL(arena)) {}  // Crash OK

  google::protobuf::Arena* absl_nonnull arena() const { return arena_; }

  absl::string_view InternString(const char* absl_nullable string) {
    return InternString(absl::NullSafeStringView(string));
  }

  absl::string_view InternString(absl::string_view string);

  absl::string_view InternString(std::string&& string);

  absl::string_view InternString(const absl::Cord& string);

 private:
  google::protobuf::Arena* absl_nonnull const arena_;
  absl::flat_hash_set<absl::string_view> strings_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_STRING_POOL_H_
