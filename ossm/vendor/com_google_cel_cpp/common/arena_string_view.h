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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_VIEW_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_VIEW_H_

#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/arena.h"

namespace cel {

class ArenaString;

// Bug in current Abseil LTS. Fixed in
// https://github.com/abseil/abseil-cpp/commit/fd7713cb9a97c49096211ff40de280b6cebbb21c
// which is not yet in an LTS.
#if defined(__clang__) && (!defined(__clang_major__) || __clang_major__ >= 13)
#define CEL_ATTRIBUTE_ARENA_STRING_VIEW ABSL_ATTRIBUTE_VIEW
#else
#define CEL_ATTRIBUTE_ARENA_STRING_VIEW
#endif

class CEL_ATTRIBUTE_ARENA_STRING_VIEW ArenaStringView final {
 public:
  using traits_type = std::char_traits<char>;
  using value_type = char;
  using pointer = char*;
  using const_pointer = const char*;
  using reference = char&;
  using const_reference = const char&;
  using const_iterator = typename absl::string_view::const_pointer;
  using iterator = typename absl::string_view::const_iterator;
  using const_reverse_iterator =
      typename absl::string_view::const_reverse_iterator;
  using reverse_iterator = typename absl::string_view::reverse_iterator;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using absl_internal_is_view = std::true_type;

  ArenaStringView() = default;
  ArenaStringView(const ArenaStringView&) = default;
  ArenaStringView& operator=(const ArenaStringView&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaStringView(
      const ArenaString& arena_string ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaStringView& operator=(
      const ArenaString& arena_string ABSL_ATTRIBUTE_LIFETIME_BOUND);

  ArenaStringView& operator=(ArenaString&&) = delete;

  explicit ArenaStringView(
      google::protobuf::Arena* absl_nullable arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : arena_(arena) {}

  ArenaStringView(std::nullptr_t) = delete;

  ArenaStringView(absl::string_view string ABSL_ATTRIBUTE_LIFETIME_BOUND,
                  google::protobuf::Arena* absl_nullable arena
                      ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : string_(string), arena_(arena) {}

  ArenaStringView(absl::string_view, std::nullptr_t) = delete;

  google::protobuf::Arena* absl_nullable arena() const { return arena_; }

  size_type size() const { return string_.size(); }

  bool empty() const { return string_.empty(); }

  size_type max_size() const { return std::numeric_limits<size_t>::max() >> 1; }

  absl_nonnull const_pointer data() const { return string_.data(); }

  const_reference front() const {
    ABSL_DCHECK(!empty());

    return string_.front();
  }

  const_reference back() const {
    ABSL_DCHECK(!empty());

    return string_.back();
  }

  const_reference operator[](size_type index) const {
    ABSL_DCHECK_LT(index, size());

    return string_[index];
  }

  void remove_prefix(size_type n) {
    ABSL_DCHECK_LE(n, size());

    string_.remove_prefix(n);
  }

  void remove_suffix(size_type n) {
    ABSL_DCHECK_LE(n, size());

    string_.remove_suffix(n);
  }

  const_iterator begin() const { return string_.begin(); }

  const_iterator cbegin() const { return string_.cbegin(); }

  const_iterator end() const { return string_.end(); }

  const_iterator cend() const { return string_.cend(); }

  const_reverse_iterator rbegin() const { return string_.rbegin(); }

  const_reverse_iterator crbegin() const { return string_.crbegin(); }

  const_reverse_iterator rend() const { return string_.rend(); }

  const_reverse_iterator crend() const { return string_.crend(); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::string_view() const { return string_; }

 private:
  absl::string_view string_;
  google::protobuf::Arena* absl_nullable arena_ = nullptr;
};

inline bool operator==(ArenaStringView lhs, ArenaStringView rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) ==
         absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator==(ArenaStringView lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) == rhs;
}

inline bool operator==(absl::string_view lhs, ArenaStringView rhs) {
  return lhs == absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator!=(ArenaStringView lhs, ArenaStringView rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) !=
         absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator!=(ArenaStringView lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) != rhs;
}

inline bool operator!=(absl::string_view lhs, ArenaStringView rhs) {
  return lhs != absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator<(ArenaStringView lhs, ArenaStringView rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) <
         absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator<(ArenaStringView lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) < rhs;
}

inline bool operator<(absl::string_view lhs, ArenaStringView rhs) {
  return lhs < absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator<=(ArenaStringView lhs, ArenaStringView rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) <=
         absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator<=(ArenaStringView lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) <= rhs;
}

inline bool operator<=(absl::string_view lhs, ArenaStringView rhs) {
  return lhs <= absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator>(ArenaStringView lhs, ArenaStringView rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) >
         absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator>(ArenaStringView lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) > rhs;
}

inline bool operator>(absl::string_view lhs, ArenaStringView rhs) {
  return lhs > absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator>=(ArenaStringView lhs, ArenaStringView rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) >=
         absl::implicit_cast<absl::string_view>(rhs);
}

inline bool operator>=(ArenaStringView lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) >= rhs;
}

inline bool operator>=(absl::string_view lhs, ArenaStringView rhs) {
  return lhs >= absl::implicit_cast<absl::string_view>(rhs);
}

template <typename H>
H AbslHashValue(H state, ArenaStringView arena_string_view) {
  return H::combine(std::move(state),
                    absl::implicit_cast<absl::string_view>(arena_string_view));
}

#undef CEL_ATTRIBUTE_ARENA_STRING_VIEW

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_VIEW_H_
