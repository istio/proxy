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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_BYTE_STRING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_BYTE_STRING_H_

#include <cstddef>
#include <cstdint>
#include <new>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/arena.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"
#include "google/protobuf/arena.h"

namespace cel {

class BytesValueInputStream;
class BytesValueOutputStream;
class StringValue;

namespace common_internal {

// absl::Cord is trivially relocatable IFF we are not using ASan or MSan. When
// using ASan or MSan absl::Cord will poison/unpoison its inline storage.
#if defined(ABSL_HAVE_ADDRESS_SANITIZER) || defined(ABSL_HAVE_MEMORY_SANITIZER)
#define CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI
#else
#define CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI ABSL_ATTRIBUTE_TRIVIAL_ABI
#endif

class CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI [[nodiscard]] ByteString;

struct ByteStringTestFriend;

enum class ByteStringKind : unsigned int {
  kSmall = 0,
  kMedium,
  kLarge,
};

inline std::ostream& operator<<(std::ostream& out, ByteStringKind kind) {
  switch (kind) {
    case ByteStringKind::kSmall:
      return out << "SMALL";
    case ByteStringKind::kMedium:
      return out << "MEDIUM";
    case ByteStringKind::kLarge:
      return out << "LARGE";
  }
}

// Representation of small strings in ByteString, which are stored in place.
struct CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI SmallByteStringRep final {
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    std::uint8_t kind : 2;
    std::uint8_t size : 6;
  };
#ifdef _MSC_VER
#pragma pack(pop)
#endif
  char data[23 - sizeof(google::protobuf::Arena*)];
  google::protobuf::Arena* absl_nullable arena;
};

inline constexpr size_t kSmallByteStringCapacity =
    sizeof(SmallByteStringRep::data);

inline constexpr size_t kMediumByteStringSizeBits = sizeof(size_t) * 8 - 2;
inline constexpr size_t kMediumByteStringMaxSize =
    (size_t{1} << kMediumByteStringSizeBits) - 1;

inline constexpr size_t kByteStringViewSizeBits = sizeof(size_t) * 8 - 1;
inline constexpr size_t kByteStringViewMaxSize =
    (size_t{1} << kByteStringViewSizeBits) - 1;

// Representation of medium strings in ByteString. These are either owned by an
// arena or managed by a reference count. This is encoded in `owner` following
// the same semantics as `cel::Owner`.
struct CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI MediumByteStringRep final {
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    size_t kind : 2;
    size_t size : kMediumByteStringSizeBits;
  };
#ifdef _MSC_VER
#pragma pack(pop)
#endif
  const char* data;
  uintptr_t owner;
};

// Representation of large strings in ByteString. These are stored as
// `absl::Cord` and never owned by an arena.
struct CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI LargeByteStringRep final {
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    size_t kind : 2;
    size_t padding : kMediumByteStringSizeBits;
  };
#ifdef _MSC_VER
#pragma pack(pop)
#endif
  alignas(absl::Cord) std::byte data[sizeof(absl::Cord)];
};

// Representation of ByteString.
union CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI ByteStringRep final {
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    ByteStringKind kind : 2;
  } header;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
  SmallByteStringRep small;
  MediumByteStringRep medium;
  LargeByteStringRep large;
};

// Returns a `absl::string_view` from `ByteString`, using `arena` to make memory
// allocations if necessary. `stable` indicates whether `cel::Value` is in a
// location where it will not be moved, so that inline string/bytes storage can
// be referenced.
absl::string_view LegacyByteString(const ByteString& string, bool stable,
                                   google::protobuf::Arena* absl_nonnull arena);

// `ByteString` is an vocabulary type capable of representing copy-on-write
// strings efficiently for arenas and reference counting. The contents of the
// byte string are owned by an arena or managed by a reference count. All byte
// strings have an associated allocator specified at construction, once the byte
// string is constructed the allocator will not and cannot change. Copying and
// moving between different allocators is supported and dealt with
// transparently by copying.
class CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI [[nodiscard]]
ByteString final {
 public:
  static ByteString Concat(const ByteString& lhs, const ByteString& rhs,
                           google::protobuf::Arena* absl_nonnull arena);

  ByteString() : ByteString(NewDeleteAllocator()) {}

  explicit ByteString(const char* absl_nullable string)
      : ByteString(NewDeleteAllocator(), string) {}

  explicit ByteString(absl::string_view string)
      : ByteString(NewDeleteAllocator(), string) {}

  explicit ByteString(const std::string& string)
      : ByteString(NewDeleteAllocator(), string) {}

  explicit ByteString(std::string&& string)
      : ByteString(NewDeleteAllocator(), std::move(string)) {}

  explicit ByteString(const absl::Cord& cord)
      : ByteString(NewDeleteAllocator(), cord) {}

  ByteString(const ByteString& other) noexcept {
    Construct(other, /*allocator=*/absl::nullopt);
  }

  ByteString(ByteString&& other) noexcept {
    Construct(other, /*allocator=*/absl::nullopt);
  }

  explicit ByteString(Allocator<> allocator) {
    SetSmallEmpty(allocator.arena());
  }

  ByteString(Allocator<> allocator, const char* absl_nullable string)
      : ByteString(allocator, absl::NullSafeStringView(string)) {}

  ByteString(Allocator<> allocator, absl::string_view string);

  ByteString(Allocator<> allocator, const std::string& string);

  ByteString(Allocator<> allocator, std::string&& string);

  ByteString(Allocator<> allocator, const absl::Cord& cord);

  ByteString(Allocator<> allocator, const ByteString& other) {
    Construct(other, allocator);
  }

  ByteString(Allocator<> allocator, ByteString&& other) {
    Construct(other, allocator);
  }

  ByteString(Borrower borrower,
             const char* absl_nullable string ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ByteString(borrower, absl::NullSafeStringView(string)) {}

  ByteString(Borrower borrower,
             absl::string_view string ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ByteString(Borrowed(borrower, string)) {}

  ByteString(Borrower borrower,
             const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ByteString(Borrowed(borrower, cord)) {}

  ~ByteString() { Destroy(); }

  ByteString& operator=(const ByteString& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      CopyFrom(other);
    }
    return *this;
  }

  ByteString& operator=(ByteString&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      MoveFrom(other);
    }
    return *this;
  }

  bool empty() const;

  size_t size() const;

  size_t max_size() const { return kByteStringViewMaxSize; }

  absl::string_view Flatten() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::optional<absl::string_view> TryFlat() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  bool Equals(absl::string_view rhs) const;
  bool Equals(const absl::Cord& rhs) const;
  bool Equals(const ByteString& rhs) const;

  int Compare(absl::string_view rhs) const;
  int Compare(const absl::Cord& rhs) const;
  int Compare(const ByteString& rhs) const;

  bool StartsWith(absl::string_view rhs) const;
  bool StartsWith(const absl::Cord& rhs) const;
  bool StartsWith(const ByteString& rhs) const;

  bool EndsWith(absl::string_view rhs) const;
  bool EndsWith(const absl::Cord& rhs) const;
  bool EndsWith(const ByteString& rhs) const;

  void RemovePrefix(size_t n);

  void RemoveSuffix(size_t n);

  std::string ToString() const;

  void CopyToString(std::string* absl_nonnull out) const;

  void AppendToString(std::string* absl_nonnull out) const;

  absl::Cord ToCord() const&;

  absl::Cord ToCord() &&;

  void CopyToCord(absl::Cord* absl_nonnull out) const;

  void AppendToCord(absl::Cord* absl_nonnull out) const;

  absl::string_view ToStringView(
      std::string* absl_nonnull scratch
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::string_view AsStringView() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  google::protobuf::Arena* absl_nullable GetArena() const;

  ByteString Clone(google::protobuf::Arena* absl_nonnull arena) const;

  void HashValue(absl::HashState state) const;

  template <typename Visitor>
  decltype(auto) Visit(Visitor&& visitor) const {
    switch (GetKind()) {
      case ByteStringKind::kSmall:
        return std::forward<Visitor>(visitor)(GetSmall());
      case ByteStringKind::kMedium:
        return std::forward<Visitor>(visitor)(GetMedium());
      case ByteStringKind::kLarge:
        return std::forward<Visitor>(visitor)(GetLarge());
    }
  }

  friend void swap(ByteString& lhs, ByteString& rhs) {
    if (&lhs != &rhs) {
      lhs.Swap(rhs);
    }
  }

  template <typename H>
  friend H AbslHashValue(H state, const ByteString& byte_string) {
    byte_string.HashValue(absl::HashState::Create(&state));
    return state;
  }

 private:
  friend class ByteStringView;
  friend struct ByteStringTestFriend;
  friend class cel::BytesValueInputStream;
  friend class cel::BytesValueOutputStream;
  friend class cel::StringValue;
  friend absl::string_view LegacyByteString(const ByteString& string,
                                            bool stable,
                                            google::protobuf::Arena* absl_nonnull arena);
  friend struct cel::ArenaTraits<ByteString>;

  static ByteString Borrowed(Borrower borrower,
                             absl::string_view string
                                 ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static ByteString Borrowed(
      Borrower borrower, const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND);

  ByteString(const ReferenceCount* absl_nonnull refcount,
             absl::string_view string);

  constexpr ByteStringKind GetKind() const { return rep_.header.kind; }

  absl::string_view GetSmall() const {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
    return GetSmall(rep_.small);
  }

  static absl::string_view GetSmall(const SmallByteStringRep& rep) {
    return absl::string_view(rep.data, rep.size);
  }

  absl::string_view GetMedium() const {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return GetMedium(rep_.medium);
  }

  static absl::string_view GetMedium(const MediumByteStringRep& rep) {
    return absl::string_view(rep.data, rep.size);
  }

  google::protobuf::Arena* absl_nullable GetSmallArena() const {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
    return GetSmallArena(rep_.small);
  }

  static google::protobuf::Arena* absl_nullable GetSmallArena(
      const SmallByteStringRep& rep) {
    return rep.arena;
  }

  google::protobuf::Arena* absl_nullable GetMediumArena() const {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return GetMediumArena(rep_.medium);
  }

  static google::protobuf::Arena* absl_nullable GetMediumArena(
      const MediumByteStringRep& rep);

  const ReferenceCount* absl_nullable GetMediumReferenceCount() const {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return GetMediumReferenceCount(rep_.medium);
  }

  static const ReferenceCount* absl_nullable GetMediumReferenceCount(
      const MediumByteStringRep& rep);

  uintptr_t GetMediumOwner() const {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return rep_.medium.owner;
  }

  absl::Cord& GetLarge() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
    return GetLarge(rep_.large);
  }

  static absl::Cord& GetLarge(
      LargeByteStringRep& rep ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return *std::launder(reinterpret_cast<absl::Cord*>(&rep.data[0]));
  }

  const absl::Cord& GetLarge() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
    return GetLarge(rep_.large);
  }

  static const absl::Cord& GetLarge(
      const LargeByteStringRep& rep ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return *std::launder(reinterpret_cast<const absl::Cord*>(&rep.data[0]));
  }

  void SetSmallEmpty(google::protobuf::Arena* absl_nullable arena) {
    rep_.header.kind = ByteStringKind::kSmall;
    rep_.small.size = 0;
    rep_.small.arena = arena;
  }

  void SetSmall(google::protobuf::Arena* absl_nullable arena, absl::string_view string);

  void SetSmall(google::protobuf::Arena* absl_nullable arena, const absl::Cord& cord);

  void SetMedium(google::protobuf::Arena* absl_nullable arena, absl::string_view string);

  void SetMedium(google::protobuf::Arena* absl_nullable arena, std::string&& string);

  void SetMedium(google::protobuf::Arena* absl_nonnull arena, const absl::Cord& cord);

  void SetMedium(absl::string_view string, uintptr_t owner);

  void SetLarge(const absl::Cord& cord);

  void SetLarge(absl::Cord&& cord);

  void Swap(ByteString& other);

  void Construct(const ByteString& other,
                 absl::optional<Allocator<>> allocator);

  void Construct(ByteString& other, absl::optional<Allocator<>> allocator);

  void CopyFrom(const ByteString& other);

  void MoveFrom(ByteString& other);

  void Destroy();

  void DestroyMedium() {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    DestroyMedium(rep_.medium);
  }

  static void DestroyMedium(const MediumByteStringRep& rep) {
    StrongUnref(GetMediumReferenceCount(rep));
  }

  void DestroyLarge() {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
    DestroyLarge(rep_.large);
  }

  static void DestroyLarge(LargeByteStringRep& rep) { GetLarge(rep).~Cord(); }

  void CopyToArray(char* absl_nonnull out) const;

  ByteStringRep rep_;
};

inline bool ByteString::Equals(const ByteString& rhs) const {
  return rhs.Visit(absl::Overload(
      [this](absl::string_view rhs) -> bool { return Equals(rhs); },
      [this](const absl::Cord& rhs) -> bool { return Equals(rhs); }));
}

inline int ByteString::Compare(const ByteString& rhs) const {
  return rhs.Visit(absl::Overload(
      [this](absl::string_view rhs) -> int { return Compare(rhs); },
      [this](const absl::Cord& rhs) -> int { return Compare(rhs); }));
}

inline bool ByteString::StartsWith(const ByteString& rhs) const {
  return rhs.Visit(absl::Overload(
      [this](absl::string_view rhs) -> bool { return StartsWith(rhs); },
      [this](const absl::Cord& rhs) -> bool { return StartsWith(rhs); }));
}

inline bool ByteString::EndsWith(const ByteString& rhs) const {
  return rhs.Visit(absl::Overload(
      [this](absl::string_view rhs) -> bool { return EndsWith(rhs); },
      [this](const absl::Cord& rhs) -> bool { return EndsWith(rhs); }));
}

inline bool operator==(const ByteString& lhs, const ByteString& rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(const ByteString& lhs, absl::string_view rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(absl::string_view lhs, const ByteString& rhs) {
  return rhs.Equals(lhs);
}

inline bool operator==(const ByteString& lhs, const absl::Cord& rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(const absl::Cord& lhs, const ByteString& rhs) {
  return rhs.Equals(lhs);
}

inline bool operator!=(const ByteString& lhs, const ByteString& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const ByteString& lhs, absl::string_view rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(absl::string_view lhs, const ByteString& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const ByteString& lhs, const absl::Cord& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const absl::Cord& lhs, const ByteString& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const ByteString& lhs, const ByteString& rhs) {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<(const ByteString& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<(absl::string_view lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) < 0;
}

inline bool operator<(const ByteString& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<(const absl::Cord& lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) < 0;
}

inline bool operator<=(const ByteString& lhs, const ByteString& rhs) {
  return lhs.Compare(rhs) <= 0;
}

inline bool operator<=(const ByteString& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs) <= 0;
}

inline bool operator<=(absl::string_view lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) <= 0;
}

inline bool operator<=(const ByteString& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs) <= 0;
}

inline bool operator<=(const absl::Cord& lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) <= 0;
}

inline bool operator>(const ByteString& lhs, const ByteString& rhs) {
  return lhs.Compare(rhs) > 0;
}

inline bool operator>(const ByteString& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs) > 0;
}

inline bool operator>(absl::string_view lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) > 0;
}

inline bool operator>(const ByteString& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs) > 0;
}

inline bool operator>(const absl::Cord& lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) > 0;
}

inline bool operator>=(const ByteString& lhs, const ByteString& rhs) {
  return lhs.Compare(rhs) >= 0;
}

inline bool operator>=(const ByteString& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs) >= 0;
}

inline bool operator>=(absl::string_view lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) >= 0;
}

inline bool operator>=(const ByteString& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs) >= 0;
}

inline bool operator>=(const absl::Cord& lhs, const ByteString& rhs) {
  return -rhs.Compare(lhs) >= 0;
}

#undef CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI

}  // namespace common_internal

template <>
struct ArenaTraits<common_internal::ByteString> {
  using constructible = std::true_type;

  static bool trivially_destructible(
      const common_internal::ByteString& byte_string) {
    switch (byte_string.GetKind()) {
      case common_internal::ByteStringKind::kSmall:
        return true;
      case common_internal::ByteStringKind::kMedium:
        return byte_string.GetMediumReferenceCount() == nullptr;
      case common_internal::ByteStringKind::kLarge:
        return false;
    }
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_BYTE_STRING_H_
