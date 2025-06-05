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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_SHARED_BYTE_STRING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_SHARED_BYTE_STRING_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/internal/arena_string.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"

namespace cel::common_internal {

class TrivialValue;

inline constexpr bool IsStringLiteral(absl::string_view string) {
#ifdef ABSL_HAVE_CONSTANT_EVALUATED
  if (!absl::is_constant_evaluated()) {
    return false;
  }
#endif
  for (const auto& c : string) {
    if (c == '\0') {
      return false;
    }
  }
  return true;
}

inline constexpr uintptr_t kByteStringReferenceCountPooledBit = uintptr_t{1}
                                                                << 0;

#ifdef _MSC_VER
#pragma pack(pack, 1)
#endif

struct ABSL_ATTRIBUTE_PACKED SharedByteStringHeader final {
  // True if the content is `absl::Cord`.
  bool is_cord : 1;
  // Only used when `is_cord` is `false`.
  size_t size : sizeof(size_t) * 8 - 1;

  SharedByteStringHeader(bool is_cord, size_t size)
      : is_cord(is_cord), size(size) {
    // Ensure size does not occupy the most significant bit.
    ABSL_DCHECK_GE(absl::bit_cast<std::make_signed_t<size_t>>(size), 0);
  }
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static_assert(sizeof(SharedByteStringHeader) == sizeof(size_t));

class SharedByteString;
class ABSL_ATTRIBUTE_TRIVIAL_ABI SharedByteStringView;

// `SharedByteString` is a compact wrapper around either an `absl::Cord` or
// `absl::string_view` with `const ReferenceCount*`.
class SharedByteString final {
 public:
  SharedByteString() noexcept : SharedByteString(absl::string_view()) {}

  explicit SharedByteString(absl::string_view string_view) noexcept
      : SharedByteString(nullptr, string_view) {}

  explicit SharedByteString(
      const std::string& string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : SharedByteString(absl::string_view(string)) {}

  explicit SharedByteString(std::string&& string)
      : SharedByteString(absl::Cord(std::move(string))) {}

  // Constructs a `SharedByteString` whose contents are `string_view` owned by
  // `refcount`. If `refcount` is not nullptr, a strong reference is taken.
  SharedByteString(const ReferenceCount* refcount,
                   absl::string_view string_view) noexcept
      : header_(false, string_view.size()) {
    content_.string.data = string_view.data();
    content_.string.refcount = reinterpret_cast<uintptr_t>(refcount);
    ABSL_ASSERT(
        (content_.string.refcount & kByteStringReferenceCountPooledBit) == 0);
    (StrongRef)(refcount);
  }

  explicit SharedByteString(absl::Cord cord) noexcept : header_(true, 0) {
    ::new (static_cast<void*>(cord_ptr())) absl::Cord(std::move(cord));
  }

  explicit SharedByteString(SharedByteStringView other) noexcept;

  SharedByteString(const SharedByteString& other) noexcept
      : header_(other.header_) {
    if (header_.is_cord) {
      ::new (static_cast<void*>(cord_ptr())) absl::Cord(*other.cord_ptr());
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
      if (IsReferenceCountedString()) {
        (StrongRef)(*GetReferenceCount());
      }
    }
  }

  SharedByteString(SharedByteString&& other) noexcept : header_(other.header_) {
    if (header_.is_cord) {
      ::new (static_cast<void*>(cord_ptr()))
          absl::Cord(std::move(*other.cord_ptr()));
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
      other.content_.string.data = "";
      other.content_.string.refcount = 0;
      other.header_.size = 0;
    }
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit SharedByteString(ArenaString string) noexcept
      : header_(false, string.size()) {
    content_.string.data = string.data();
    content_.string.refcount = kByteStringReferenceCountPooledBit;
  }

  // Constructs a shared byte string using `allocator` to allocate memory.
  SharedByteString(Allocator<> allocator, absl::string_view value);

  // Constructs a shared byte string using `allocator` to allocate memory,
  // if necessary.
  SharedByteString(Allocator<> allocator, const absl::Cord& value);

  // Constructs a shared byte string which is borrowed and references `value`.
  SharedByteString(Borrower borrower, absl::string_view value)
      : SharedByteString(common_internal::BorrowerRelease(borrower), value) {}

  // Constructs a shared byte string which is borrowed and references `value`.
  SharedByteString(Borrower, const absl::Cord& value)
      : SharedByteString(value) {}

  ~SharedByteString() noexcept {
    if (header_.is_cord) {
      cord_ptr()->~Cord();
    } else {
      if (IsReferenceCountedString()) {
        (StrongUnref)(*GetReferenceCount());
      }
    }
  }

  SharedByteString& operator=(const SharedByteString& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      this->~SharedByteString();
      ::new (static_cast<void*>(this)) SharedByteString(other);
    }
    return *this;
  }

  SharedByteString& operator=(SharedByteString&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      this->~SharedByteString();
      ::new (static_cast<void*>(this)) SharedByteString(std::move(other));
    }
    return *this;
  }

  SharedByteString Clone(Allocator<> allocator) const;

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  Visit(Visitor&& visitor) const {
    if (header_.is_cord) {
      return std::forward<Visitor>(visitor)(*cord_ptr());
    } else {
      return std::forward<Visitor>(visitor)(
          absl::string_view(content_.string.data, header_.size));
    }
  }

  void swap(SharedByteString& other) noexcept {
    using std::swap;
    if (header_.is_cord) {
      // absl::Cord
      if (other.header_.is_cord) {
        // absl::Cord
        swap(*cord_ptr(), *other.cord_ptr());
      } else {
        // absl::string_view
        SwapMixed(*this, other);
      }
    } else {
      // absl::string_view
      if (other.header_.is_cord) {
        // absl::Cord
        SwapMixed(other, *this);
      } else {
        // absl::string_view
        swap(content_.string.data, other.content_.string.data);
        swap(content_.string.refcount, other.content_.string.refcount);
      }
    }
    swap(header_, other.header_);
  }

  // Retrieves the contents of this byte string as `absl::string_view`. If this
  // byte string is backed by an `absl::Cord` which is not flat, `scratch` is
  // used to store the contents and the returned `absl::string_view` is a view
  // of `scratch`.
  absl::string_view ToString(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return Visit(absl::Overload(
        [](absl::string_view string) -> absl::string_view { return string; },
        [&scratch](const absl::Cord& cord) -> absl::string_view {
          if (auto flat = cord.TryFlat(); flat.has_value()) {
            return *flat;
          }
          scratch = static_cast<std::string>(cord);
          return absl::string_view(scratch);
        }));
  }

  std::string ToString() const {
    return Visit(absl::Overload(
        [](absl::string_view string) -> std::string {
          return std::string(string);
        },
        [](const absl::Cord& cord) -> std::string {
          return static_cast<std::string>(cord);
        }));
  }

  absl::string_view AsStringView() const {
    ABSL_DCHECK(!header_.is_cord);
    return absl::string_view(content_.string.data, header_.size);
  }

  absl::Cord ToCord() const {
    return Visit(absl::Overload(
        [this](absl::string_view string) -> absl::Cord {
          if (IsReferenceCountedString()) {
            const auto* refcount = GetReferenceCount();
            (StrongRef)(*refcount);
            return absl::MakeCordFromExternal(
                string, [refcount]() { (StrongUnref)(*refcount); });
          }
          return absl::Cord(string);
        },
        [](const absl::Cord& cord) -> absl::Cord { return cord; }));
  }

  template <typename H>
  friend H AbslHashValue(H state, const SharedByteString& byte_string) {
    if (byte_string.header_.is_cord) {
      return H::combine(std::move(state), *byte_string.cord_ptr());
    } else {
      return H::combine(std::move(state),
                        absl::string_view(byte_string.content_.string.data,
                                          byte_string.header_.size));
    }
  }

  friend bool operator==(const SharedByteString& lhs,
                         const SharedByteString& rhs) {
    if (lhs.header_.is_cord) {
      if (rhs.header_.is_cord) {
        return *lhs.cord_ptr() == *rhs.cord_ptr();
      } else {
        return *lhs.cord_ptr() ==
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    } else {
      if (rhs.header_.is_cord) {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) ==
               *rhs.cord_ptr();
      } else {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) ==
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    }
  }

  friend bool operator<(const SharedByteString& lhs,
                        const SharedByteString& rhs) {
    if (lhs.header_.is_cord) {
      if (rhs.header_.is_cord) {
        return *lhs.cord_ptr() < *rhs.cord_ptr();
      } else {
        return *lhs.cord_ptr() <
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    } else {
      if (rhs.header_.is_cord) {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) <
               *rhs.cord_ptr();
      } else {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) <
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    }
  }

  bool IsPooledString() const {
    return !header_.is_cord &&
           (content_.string.refcount & kByteStringReferenceCountPooledBit) != 0;
  }

 private:
  friend class TrivialValue;
  friend class SharedByteStringView;

  static void SwapMixed(SharedByteString& cord,
                        SharedByteString& string) noexcept {
    const auto* string_data = string.content_.string.data;
    const auto string_refcount = string.content_.string.refcount;
    ::new (static_cast<void*>(string.cord_ptr()))
        absl::Cord(std::move(*cord.cord_ptr()));
    cord.cord_ptr()->~Cord();
    cord.content_.string.data = string_data;
    cord.content_.string.refcount = string_refcount;
  }

  bool IsManagedString() const {
    ABSL_ASSERT(!header_.is_cord);
    return content_.string.refcount != 0;
  }

  bool IsReferenceCountedString() const {
    return IsManagedString() &&
           (content_.string.refcount & kByteStringReferenceCountPooledBit) == 0;
  }

  const ReferenceCount* GetReferenceCount() const {
    ABSL_ASSERT(IsReferenceCountedString());
    return reinterpret_cast<const ReferenceCount*>(content_.string.refcount);
  }

  absl::Cord* cord_ptr() noexcept {
    return reinterpret_cast<absl::Cord*>(&content_.cord[0]);
  }

  const absl::Cord* cord_ptr() const noexcept {
    return reinterpret_cast<const absl::Cord*>(&content_.cord[0]);
  }

  SharedByteStringHeader header_;
  union {
    struct {
      const char* data;
      uintptr_t refcount;
    } string;
    alignas(absl::Cord) char cord[sizeof(absl::Cord)];
  } content_;
};

inline void swap(SharedByteString& lhs, SharedByteString& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator!=(const SharedByteString& lhs,
                       const SharedByteString& rhs) {
  return !operator==(lhs, rhs);
}

class ABSL_ATTRIBUTE_TRIVIAL_ABI SharedByteStringView final {
 public:
  SharedByteStringView() noexcept : SharedByteStringView(absl::string_view()) {}

  explicit SharedByteStringView(absl::string_view string) noexcept
      : SharedByteStringView(nullptr, string) {}

  explicit SharedByteStringView(
      const std::string& string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : SharedByteStringView(absl::string_view(string)) {}

  SharedByteStringView(const ReferenceCount* refcount,
                       absl::string_view string) noexcept
      : header_(false, string.size()) {
    content_.string.data = string.data();
    content_.string.refcount = reinterpret_cast<uintptr_t>(refcount);
  }

  explicit SharedByteStringView(
      const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : header_(true, 0) {
    content_.cord = &cord;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedByteStringView(
      const SharedByteString& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : header_(other.header_) {
    if (header_.is_cord) {
      content_.cord = other.cord_ptr();
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
    }
  }

  explicit SharedByteStringView(ArenaString string) noexcept
      : header_(false, string.size()) {
    content_.string.data = string.data();
    content_.string.refcount = kByteStringReferenceCountPooledBit;
  }

  SharedByteStringView(const SharedByteStringView&) = default;
  SharedByteStringView& operator=(const SharedByteStringView&) = default;

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  Visit(Visitor&& visitor) const {
    if (header_.is_cord) {
      return std::forward<Visitor>(visitor)(*content_.cord);
    } else {
      return std::forward<Visitor>(visitor)(
          absl::string_view(content_.string.data, header_.size));
    }
  }

  void swap(SharedByteStringView& other) noexcept {
    using std::swap;
    swap(header_, other.header_);
    swap(content_, other.content_);
  }

  // Retrieves the contents of this byte string as `absl::string_view`. If this
  // byte string is backed by an `absl::Cord` which is not flat, `scratch` is
  // used to store the contents and the returned `absl::string_view` is a view
  // of `scratch`.
  absl::string_view ToString(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return Visit(absl::Overload(
        [](absl::string_view string) -> absl::string_view { return string; },
        [&scratch](const absl::Cord& cord) -> absl::string_view {
          if (auto flat = cord.TryFlat(); flat.has_value()) {
            return *flat;
          }
          scratch = static_cast<std::string>(cord);
          return absl::string_view(scratch);
        }));
  }

  std::string ToString() const {
    return Visit(absl::Overload(
        [](absl::string_view string) -> std::string {
          return std::string(string);
        },
        [](const absl::Cord& cord) -> std::string {
          return static_cast<std::string>(cord);
        }));
  }

  absl::string_view AsStringView() const {
    ABSL_DCHECK(!header_.is_cord);
    return absl::string_view(content_.string.data, header_.size);
  }

  absl::Cord ToCord() const {
    return Visit(absl::Overload(
        [this](absl::string_view string) -> absl::Cord {
          if (IsReferenceCountedString()) {
            const auto* refcount = GetReferenceCount();
            (StrongRef)(*refcount);
            return absl::MakeCordFromExternal(
                string, [refcount]() { (StrongUnref)(*refcount); });
          }
          return absl::Cord(string);
        },
        [](const absl::Cord& cord) -> absl::Cord { return cord; }));
  }

  template <typename H>
  friend H AbslHashValue(H state, SharedByteStringView byte_string) {
    if (byte_string.header_.is_cord) {
      return H::combine(std::move(state), *byte_string.content_.cord);
    } else {
      return H::combine(std::move(state),
                        absl::string_view(byte_string.content_.string.data,
                                          byte_string.header_.size));
    }
  }

  friend bool operator==(SharedByteStringView lhs, SharedByteStringView rhs) {
    if (lhs.header_.is_cord) {
      if (rhs.header_.is_cord) {
        return *lhs.content_.cord == *rhs.content_.cord;
      } else {
        return *lhs.content_.cord ==
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    } else {
      if (rhs.header_.is_cord) {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) ==
               *rhs.content_.cord;
      } else {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) ==
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    }
  }

  friend bool operator<(SharedByteStringView lhs, SharedByteStringView rhs) {
    if (lhs.header_.is_cord) {
      if (rhs.header_.is_cord) {
        return *lhs.content_.cord < *rhs.content_.cord;
      } else {
        return *lhs.content_.cord <
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    } else {
      if (rhs.header_.is_cord) {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) <
               *rhs.content_.cord;
      } else {
        return absl::string_view(lhs.content_.string.data, lhs.header_.size) <
               absl::string_view(rhs.content_.string.data, rhs.header_.size);
      }
    }
  }

  bool IsPooledString() const {
    return !header_.is_cord &&
           (content_.string.refcount & kByteStringReferenceCountPooledBit) != 0;
  }

 private:
  friend class SharedByteString;

  bool IsManagedString() const {
    ABSL_ASSERT(!header_.is_cord);
    return content_.string.refcount != 0;
  }

  bool IsReferenceCountedString() const {
    return IsManagedString() &&
           (content_.string.refcount & kByteStringReferenceCountPooledBit) == 0;
  }

  const ReferenceCount* GetReferenceCount() const {
    ABSL_ASSERT(IsReferenceCountedString());
    return reinterpret_cast<const ReferenceCount*>(content_.string.refcount);
  }

  SharedByteStringHeader header_;
  union {
    struct {
      const char* data;
      uintptr_t refcount;
    } string;
    const absl::Cord* cord;
  } content_;
};

inline bool operator!=(SharedByteStringView lhs, SharedByteStringView rhs) {
  return !operator==(lhs, rhs);
}

inline SharedByteString::SharedByteString(SharedByteStringView other) noexcept
    : header_(other.header_) {
  if (header_.is_cord) {
    ::new (static_cast<void*>(cord_ptr())) absl::Cord(*other.content_.cord);
  } else {
    if (other.content_.string.refcount == 0) {
      // Unfortunately since we cannot guarantee lifetimes when using arenas or
      // without a reference count, we are forced to transform this into a cord.
      header_.is_cord = true;
      header_.size = 0;
      ::new (static_cast<void*>(cord_ptr())) absl::Cord(
          absl::string_view(other.content_.string.data, other.header_.size));
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
      if (IsReferenceCountedString()) {
        (StrongRef)(*GetReferenceCount());
      }
    }
  }
}

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_SHARED_BYTE_STRING_H_
