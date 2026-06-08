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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "common/arena_string_view.h"
#include "google/protobuf/arena.h"

namespace cel {

class ArenaStringPool;

// Bug in current Abseil LTS. Fixed in
// https://github.com/abseil/abseil-cpp/commit/fd7713cb9a97c49096211ff40de280b6cebbb21c
// which is not yet in an LTS.
#if defined(__clang__) && (!defined(__clang_major__) || __clang_major__ >= 13)
#define CEL_ATTRIBUTE_ARENA_STRING_OWNER ABSL_ATTRIBUTE_OWNER
#else
#define CEL_ATTRIBUTE_ARENA_STRING_OWNER
#endif

namespace common_internal {

enum class ArenaStringKind : unsigned int {
  kSmall = 0,
  kLarge,
};

struct ArenaStringSmallRep final {
  ArenaStringKind kind : 1;
  uint8_t size : 7;
  char data[23 - sizeof(google::protobuf::Arena*)];
  google::protobuf::Arena* absl_nullable arena;
};

struct ArenaStringLargeRep final {
  ArenaStringKind kind : 1;
  size_t size : sizeof(size_t) * 8 - 1;
  const char* absl_nonnull data;
  google::protobuf::Arena* absl_nullable arena;
};

inline constexpr size_t kArenaStringSmallCapacity =
    sizeof(ArenaStringSmallRep::data);

union ArenaStringRep final {
  struct {
    ArenaStringKind kind : 1;
  };
  ArenaStringSmallRep small;
  ArenaStringLargeRep large;
};

}  // namespace common_internal

// `ArenaString` is a read-only string which is either backed by a static string
// literal or owned by the `ArenaStringPool` that created it. It is compatible
// with `absl::string_view` and is implicitly convertible to it.
class CEL_ATTRIBUTE_ARENA_STRING_OWNER ArenaString final {
 public:
  using traits_type = std::char_traits<char>;
  using value_type = char;
  using pointer = char*;
  using const_pointer = const char*;
  using reference = char&;
  using const_reference = const char&;
  using const_iterator = const_pointer;
  using iterator = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using absl_internal_is_view = std::false_type;

  ArenaString() : ArenaString(static_cast<google::protobuf::Arena*>(nullptr)) {}

  ArenaString(const ArenaString&) = default;
  ArenaString& operator=(const ArenaString&) = default;

  explicit ArenaString(
      google::protobuf::Arena* absl_nullable arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ArenaString(absl::string_view(), arena) {}

  ArenaString(std::nullptr_t) = delete;

  ArenaString(absl::string_view string, google::protobuf::Arena* absl_nullable arena
                                            ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    if (string.size() <= common_internal::kArenaStringSmallCapacity) {
      rep_.small.kind = common_internal::ArenaStringKind::kSmall;
      rep_.small.size = string.size();
      std::memcpy(rep_.small.data, string.data(), string.size());
      rep_.small.arena = arena;
    } else {
      rep_.large.kind = common_internal::ArenaStringKind::kLarge;
      rep_.large.size = string.size();
      rep_.large.data = string.data();
      rep_.large.arena = arena;
    }
  }

  ArenaString(absl::string_view, std::nullptr_t) = delete;

  explicit ArenaString(ArenaStringView other)
      : ArenaString(absl::implicit_cast<absl::string_view>(other),
                    other.arena()) {}

  google::protobuf::Arena* absl_nullable arena() const {
    switch (rep_.kind) {
      case common_internal::ArenaStringKind::kSmall:
        return rep_.small.arena;
      case common_internal::ArenaStringKind::kLarge:
        return rep_.large.arena;
    }
  }

  size_type size() const {
    switch (rep_.kind) {
      case common_internal::ArenaStringKind::kSmall:
        return rep_.small.size;
      case common_internal::ArenaStringKind::kLarge:
        return rep_.large.size;
    }
  }

  bool empty() const { return size() == 0; }

  size_type max_size() const { return std::numeric_limits<size_t>::max() >> 1; }

  absl_nonnull const_pointer data() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    switch (rep_.kind) {
      case common_internal::ArenaStringKind::kSmall:
        return rep_.small.data;
      case common_internal::ArenaStringKind::kLarge:
        return rep_.large.data;
    }
  }

  const_reference front() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!empty());

    return data()[0];
  }

  const_reference back() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!empty());

    return data()[size() - 1];
  }

  const_reference operator[](size_type index) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_LT(index, size());

    return data()[index];
  }

  void remove_prefix(size_type n) {
    ABSL_DCHECK_LE(n, size());

    switch (rep_.kind) {
      case common_internal::ArenaStringKind::kSmall:
        std::memmove(rep_.small.data, rep_.small.data + n, rep_.small.size - n);
        rep_.small.size = rep_.small.size - n;
        break;
      case common_internal::ArenaStringKind::kLarge:
        rep_.large.data += n;
        rep_.large.size = rep_.large.size - n;
        break;
    }
  }

  void remove_suffix(size_type n) {
    ABSL_DCHECK_LE(n, size());

    switch (rep_.kind) {
      case common_internal::ArenaStringKind::kSmall:
        rep_.small.size = rep_.small.size - n;
        break;
      case common_internal::ArenaStringKind::kLarge:
        rep_.large.size = rep_.large.size - n;
        break;
    }
  }

  const_iterator begin() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return data(); }

  const_iterator cbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return begin();
  }

  const_iterator end() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return data() + size();
  }

  const_iterator cend() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return end(); }

  const_reverse_iterator rbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::make_reverse_iterator(end());
  }

  const_reverse_iterator crbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rbegin();
  }

  const_reverse_iterator rend() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::make_reverse_iterator(begin());
  }

  const_reverse_iterator crend() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rend();
  }

 private:
  friend class ArenaStringView;

  common_internal::ArenaStringRep rep_;
};

inline ArenaStringView::ArenaStringView(
    const ArenaString& arena_string ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  switch (arena_string.rep_.kind) {
    case common_internal::ArenaStringKind::kSmall:
      string_ = absl::string_view(arena_string.rep_.small.data,
                                  arena_string.rep_.small.size);
      arena_ = arena_string.rep_.small.arena;
      break;
    case common_internal::ArenaStringKind::kLarge:
      string_ = absl::string_view(arena_string.rep_.large.data,
                                  arena_string.rep_.large.size);
      arena_ = arena_string.rep_.large.arena;
      break;
  }
}

inline ArenaStringView& ArenaStringView::operator=(
    const ArenaString& arena_string ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  switch (arena_string.rep_.kind) {
    case common_internal::ArenaStringKind::kSmall:
      string_ = absl::string_view(arena_string.rep_.small.data,
                                  arena_string.rep_.small.size);
      arena_ = arena_string.rep_.small.arena;
      break;
    case common_internal::ArenaStringKind::kLarge:
      string_ = absl::string_view(arena_string.rep_.large.data,
                                  arena_string.rep_.large.size);
      arena_ = arena_string.rep_.large.arena;
      break;
  }
  return *this;
}

inline bool operator==(const ArenaString& lhs, const ArenaString& rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) ==
         absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator==(const ArenaString& lhs, absl::string_view rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) == rhs;
}

inline bool operator==(absl::string_view lhs, const ArenaString& rhs) {
  return lhs == absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator!=(const ArenaString& lhs, const ArenaString& rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) !=
         absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator!=(const ArenaString& lhs, absl::string_view rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) != rhs;
}

inline bool operator!=(absl::string_view lhs, const ArenaString& rhs) {
  return lhs != absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator<(const ArenaString& lhs, const ArenaString& rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) <
         absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator<(const ArenaString& lhs, absl::string_view rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) < rhs;
}

inline bool operator<(absl::string_view lhs, const ArenaString& rhs) {
  return lhs < absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator<=(const ArenaString& lhs, const ArenaString& rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) <=
         absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator<=(const ArenaString& lhs, absl::string_view rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) <= rhs;
}

inline bool operator<=(absl::string_view lhs, const ArenaString& rhs) {
  return lhs <= absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator>(const ArenaString& lhs, const ArenaString& rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) >
         absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator>(const ArenaString& lhs, absl::string_view rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) > rhs;
}

inline bool operator>(absl::string_view lhs, const ArenaString& rhs) {
  return lhs > absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator>=(const ArenaString& lhs, const ArenaString& rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) >=
         absl::implicit_cast<ArenaStringView>(rhs);
}

inline bool operator>=(const ArenaString& lhs, absl::string_view rhs) {
  return absl::implicit_cast<ArenaStringView>(lhs) >= rhs;
}

inline bool operator>=(absl::string_view lhs, const ArenaString& rhs) {
  return lhs >= absl::implicit_cast<ArenaStringView>(rhs);
}

template <typename H>
H AbslHashValue(H state, const ArenaString& arena_string) {
  return H::combine(std::move(state),
                    absl::implicit_cast<ArenaStringView>(arena_string));
}

#undef CEL_ATTRIBUTE_ARENA_STRING_OWNER

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_STRING_H_
