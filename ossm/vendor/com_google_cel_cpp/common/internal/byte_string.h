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
#include <functional>
#include <new>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/internal/metadata.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

// absl::Cord is trivially relocatable IFF we are not using ASan or MSan. When
// using ASan or MSan absl::Cord will poison/unpoison its inline storage.
#if defined(ABSL_HAVE_ADDRESS_SANITIZER) || defined(ABSL_HAVE_MEMORY_SANITIZER)
#define CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI
#else
#define CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI ABSL_ATTRIBUTE_TRIVIAL_ABI
#endif

class CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI [[nodiscard]] ByteString;
class ByteStringView;

struct ByteStringTestFriend;
struct ByteStringViewTestFriend;

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
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    ByteStringKind kind : 2;
    size_t size : 6;
  };
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  char data[23 - sizeof(google::protobuf::Arena*)];
  google::protobuf::Arena* arena;
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
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    ByteStringKind kind : 2;
    size_t size : kMediumByteStringSizeBits;
  };
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  const char* data;
  uintptr_t owner;
};

// Representation of large strings in ByteString. These are stored as
// `absl::Cord` and never owned by an arena.
struct CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI LargeByteStringRep final {
#ifdef _MSC_VER
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    ByteStringKind kind : 2;
    size_t padding : kMediumByteStringSizeBits;
  };
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  alignas(absl::Cord) char data[sizeof(absl::Cord)];
};

// Representation of ByteString.
union CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI ByteStringRep final {
#ifdef _MSC_VER
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI {
    ByteStringKind kind : 2;
  } header;
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  SmallByteStringRep small;
  MediumByteStringRep medium;
  LargeByteStringRep large;
};

// `ByteString` is an vocabulary type capable of representing copy-on-write
// strings efficiently for arenas and reference counting. The contents of the
// byte string are owned by an arena or managed by a reference count. All byte
// strings have an associated allocator specified at construction, once the byte
// string is constructed the allocator will not and cannot change. Copying and
// moving between different allocators is supported and dealt with
// transparently by copying.
class CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI
    [[nodiscard]] ByteString final {
 public:
  static ByteString Owned(Allocator<> allocator, const char* string) {
    return ByteString(allocator, string);
  }

  static ByteString Owned(Allocator<> allocator, absl::string_view string) {
    return ByteString(allocator, string);
  }

  static ByteString Owned(Allocator<> allocator, const std::string& string) {
    return ByteString(allocator, string);
  }

  static ByteString Owned(Allocator<> allocator, std::string&& string) {
    return ByteString(allocator, std::move(string));
  }

  static ByteString Owned(Allocator<> allocator, const absl::Cord& cord) {
    return ByteString(allocator, cord);
  }

  static ByteString Owned(Allocator<> allocator, ByteStringView other);

  static ByteString Borrowed(
      Owner owner, absl::string_view string ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static ByteString Borrowed(
      const Owner& owner, const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND);

  ByteString() noexcept : ByteString(NewDeleteAllocator()) {}

  explicit ByteString(const char* string)
      : ByteString(NewDeleteAllocator(), string) {}

  explicit ByteString(absl::string_view string)
      : ByteString(NewDeleteAllocator(), string) {}

  explicit ByteString(const std::string& string)
      : ByteString(NewDeleteAllocator(), string) {}

  explicit ByteString(std::string&& string)
      : ByteString(NewDeleteAllocator(), std::move(string)) {}

  explicit ByteString(const absl::Cord& cord)
      : ByteString(NewDeleteAllocator(), cord) {}

  explicit ByteString(ByteStringView other);

  ByteString(const ByteString& other) : ByteString(other.GetArena(), other) {}

  ByteString(ByteString&& other)
      : ByteString(other.GetArena(), std::move(other)) {}

  explicit ByteString(Allocator<> allocator) noexcept {
    SetSmallEmpty(allocator.arena());
  }

  ByteString(Allocator<> allocator, const char* string)
      : ByteString(allocator, absl::NullSafeStringView(string)) {}

  ByteString(Allocator<> allocator, absl::string_view string);

  ByteString(Allocator<> allocator, const std::string& string);

  ByteString(Allocator<> allocator, std::string&& string);

  ByteString(Allocator<> allocator, const absl::Cord& cord);

  ByteString(Allocator<> allocator, ByteStringView other);

  ByteString(Allocator<> allocator, const ByteString& other)
      : ByteString(allocator) {
    CopyFrom(other);
  }

  ByteString(Allocator<> allocator, ByteString&& other)
      : ByteString(allocator) {
    MoveFrom(other);
  }

  ~ByteString() { Destroy(); }

  ByteString& operator=(const ByteString& other) {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      CopyFrom(other);
    }
    return *this;
  }

  ByteString& operator=(ByteString&& other) {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      MoveFrom(other);
    }
    return *this;
  }

  ByteString& operator=(ByteStringView other);

  bool empty() const noexcept;

  size_t size() const noexcept;

  size_t max_size() const noexcept { return kByteStringViewMaxSize; }

  absl::string_view Flatten() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::optional<absl::string_view> TryFlat() const noexcept
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::string_view GetFlat(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  bool Equals(ByteStringView rhs) const noexcept;

  int Compare(ByteStringView rhs) const noexcept;

  bool StartsWith(ByteStringView rhs) const noexcept;

  bool EndsWith(ByteStringView rhs) const noexcept;

  void RemovePrefix(size_t n);

  void RemoveSuffix(size_t n);

  std::string ToString() const;

  absl::Cord ToCord() const&;

  absl::Cord ToCord() &&;

  absl::Nullable<google::protobuf::Arena*> GetArena() const noexcept;

  void HashValue(absl::HashState state) const;

  void swap(ByteString& other) {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      Swap(other);
    }
  }

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  Visit(Visitor&& visitor) const {
    switch (GetKind()) {
      case ByteStringKind::kSmall:
        return std::invoke(std::forward<Visitor>(visitor), GetSmall());
      case ByteStringKind::kMedium:
        return std::invoke(std::forward<Visitor>(visitor), GetMedium());
      case ByteStringKind::kLarge:
        return std::invoke(std::forward<Visitor>(visitor), GetLarge());
    }
  }

  friend void swap(ByteString& lhs, ByteString& rhs) { lhs.swap(rhs); }

 private:
  friend class ByteStringView;
  friend struct ByteStringTestFriend;

  ByteString(absl::Nonnull<const ReferenceCount*> refcount,
             absl::string_view string);

  constexpr ByteStringKind GetKind() const noexcept { return rep_.header.kind; }

  absl::string_view GetSmall() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
    return GetSmall(rep_.small);
  }

  static absl::string_view GetSmall(const SmallByteStringRep& rep) noexcept {
    return absl::string_view(rep.data, rep.size);
  }

  absl::string_view GetMedium() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return GetMedium(rep_.medium);
  }

  static absl::string_view GetMedium(const MediumByteStringRep& rep) noexcept {
    return absl::string_view(rep.data, rep.size);
  }

  absl::Nullable<google::protobuf::Arena*> GetSmallArena() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
    return GetSmallArena(rep_.small);
  }

  static absl::Nullable<google::protobuf::Arena*> GetSmallArena(
      const SmallByteStringRep& rep) noexcept {
    return rep.arena;
  }

  absl::Nullable<google::protobuf::Arena*> GetMediumArena() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return GetMediumArena(rep_.medium);
  }

  static absl::Nullable<google::protobuf::Arena*> GetMediumArena(
      const MediumByteStringRep& rep) noexcept;

  absl::Nullable<const ReferenceCount*> GetMediumReferenceCount()
      const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return GetMediumReferenceCount(rep_.medium);
  }

  static absl::Nullable<const ReferenceCount*> GetMediumReferenceCount(
      const MediumByteStringRep& rep) noexcept;

  uintptr_t GetMediumOwner() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    return rep_.medium.owner;
  }

  absl::Cord& GetLarge() noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
    return GetLarge(rep_.large);
  }

  static absl::Cord& GetLarge(
      LargeByteStringRep& rep ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *std::launder(reinterpret_cast<absl::Cord*>(&rep.data[0]));
  }

  const absl::Cord& GetLarge() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
    return GetLarge(rep_.large);
  }

  static const absl::Cord& GetLarge(
      const LargeByteStringRep& rep ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *std::launder(reinterpret_cast<const absl::Cord*>(&rep.data[0]));
  }

  void SetSmallEmpty(absl::Nullable<google::protobuf::Arena*> arena);

  void SetSmall(absl::Nullable<google::protobuf::Arena*> arena, absl::string_view string);

  void SetSmall(absl::Nullable<google::protobuf::Arena*> arena, const absl::Cord& cord);

  void SetMedium(absl::Nullable<google::protobuf::Arena*> arena,
                 absl::string_view string);

  void SetMedium(absl::Nullable<google::protobuf::Arena*> arena, std::string&& string);

  void SetMedium(absl::Nonnull<google::protobuf::Arena*> arena, const absl::Cord& cord);

  void SetMedium(absl::string_view string, uintptr_t owner);

  void SetMediumOrLarge(absl::Nullable<google::protobuf::Arena*> arena,
                        const absl::Cord& cord);

  void SetMediumOrLarge(absl::Nullable<google::protobuf::Arena*> arena,
                        absl::Cord&& cord);

  void SetLarge(const absl::Cord& cord);

  void SetLarge(absl::Cord&& cord);

  void Swap(ByteString& other);

  static void SwapSmallSmall(ByteString& lhs, ByteString& rhs);
  static void SwapSmallMedium(ByteString& lhs, ByteString& rhs);
  static void SwapSmallLarge(ByteString& lhs, ByteString& rhs);
  static void SwapMediumMedium(ByteString& lhs, ByteString& rhs);
  static void SwapMediumLarge(ByteString& lhs, ByteString& rhs);
  static void SwapLargeLarge(ByteString& lhs, ByteString& rhs);

  void CopyFrom(const ByteString& other);

  void CopyFromSmallSmall(const ByteString& other);
  void CopyFromSmallMedium(const ByteString& other);
  void CopyFromSmallLarge(const ByteString& other);
  void CopyFromMediumSmall(const ByteString& other);
  void CopyFromMediumMedium(const ByteString& other);
  void CopyFromMediumLarge(const ByteString& other);
  void CopyFromLargeSmall(const ByteString& other);
  void CopyFromLargeMedium(const ByteString& other);
  void CopyFromLargeLarge(const ByteString& other);

  void CopyFrom(ByteStringView other);

  void CopyFromSmallString(ByteStringView other);
  void CopyFromSmallCord(ByteStringView other);
  void CopyFromMediumString(ByteStringView other);
  void CopyFromMediumCord(ByteStringView other);
  void CopyFromLargeString(ByteStringView other);
  void CopyFromLargeCord(ByteStringView other);

  void MoveFrom(ByteString& other);

  void MoveFromSmallSmall(ByteString& other);
  void MoveFromSmallMedium(ByteString& other);
  void MoveFromSmallLarge(ByteString& other);
  void MoveFromMediumSmall(ByteString& other);
  void MoveFromMediumMedium(ByteString& other);
  void MoveFromMediumLarge(ByteString& other);
  void MoveFromLargeSmall(ByteString& other);
  void MoveFromLargeMedium(ByteString& other);
  void MoveFromLargeLarge(ByteString& other);

  void Destroy() noexcept;

  void DestroyMedium() noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
    DestroyMedium(rep_.medium);
  }

  static void DestroyMedium(const MediumByteStringRep& rep) noexcept {
    StrongUnref(GetMediumReferenceCount(rep));
  }

  void DestroyLarge() noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
    DestroyLarge(rep_.large);
  }

  static void DestroyLarge(LargeByteStringRep& rep) noexcept {
    GetLarge(rep).~Cord();
  }

  ByteStringRep rep_;
};

template <typename H>
H AbslHashValue(H state, const ByteString& byte_string) {
  byte_string.HashValue(absl::HashState::Create(&state));
  return state;
}

enum class ByteStringViewKind : unsigned int {
  kString = 0,
  kCord,
};

inline std::ostream& operator<<(std::ostream& out, ByteStringViewKind kind) {
  switch (kind) {
    case ByteStringViewKind::kString:
      return out << "STRING";
    case ByteStringViewKind::kCord:
      return out << "CORD";
  }
}

struct StringByteStringViewRep final {
#ifdef _MSC_VER
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED {
    ByteStringViewKind kind : 1;
    size_t size : kByteStringViewSizeBits;
  };
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  const char* data;
  uintptr_t owner;
};

struct CordByteStringViewRep final {
#ifdef _MSC_VER
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED {
    ByteStringViewKind kind : 1;
    size_t size : kByteStringViewSizeBits;
  };
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  const absl::Cord* data;
  size_t pos;
};

union ByteStringViewRep final {
#ifdef _MSC_VER
#pragma push(pack, 1)
#endif
  struct ABSL_ATTRIBUTE_PACKED {
    ByteStringViewKind kind : 1;
    size_t size : kByteStringViewSizeBits;
  } header;
#ifdef _MSC_VER
#pragma pop(pack)
#endif
  StringByteStringViewRep string;
  CordByteStringViewRep cord;
};

// `ByteStringView` is to `ByteString` what `std::string_view` is to
// `std::string`. While it is capable of being a view over the underlying data
// of `ByteStringView`, it is also capable of being a view over `std::string`,
// `std::string_view`, and `absl::Cord`.
class ByteStringView final {
 public:
  ByteStringView() noexcept {
    rep_.header.kind = ByteStringViewKind::kString;
    rep_.string.size = 0;
    rep_.string.data = "";
    rep_.string.owner = 0;
  }

  ByteStringView(const ByteStringView&) = default;
  ByteStringView(ByteStringView&&) = default;
  ByteStringView& operator=(const ByteStringView&) = default;
  ByteStringView& operator=(ByteStringView&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView(const char* string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : ByteStringView(absl::NullSafeStringView(string)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView(
      absl::string_view string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ABSL_DCHECK_LE(string.size(), max_size());
    rep_.header.kind = ByteStringViewKind::kString;
    rep_.string.size = string.size();
    rep_.string.data = string.data();
    rep_.string.owner = 0;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView(
      const std::string& string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : ByteStringView(absl::string_view(string)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView(
      const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ABSL_DCHECK_LE(cord.size(), max_size());
    rep_.header.kind = ByteStringViewKind::kCord;
    rep_.cord.size = cord.size();
    rep_.cord.data = &cord;
    rep_.cord.pos = 0;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView(
      const ByteString& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView& operator=(
      const char* string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *this = ByteStringView(string);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView& operator=(
      absl::string_view string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *this = ByteStringView(string);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView& operator=(
      const std::string& string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *this = ByteStringView(string);
  }

  ByteStringView& operator=(std::string&&) = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView& operator=(
      const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *this = ByteStringView(cord);
  }

  ByteStringView& operator=(absl::Cord&&) = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ByteStringView& operator=(
      const ByteString& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return *this = ByteStringView(other);
  }

  ByteStringView& operator=(ByteString&&) = delete;

  bool empty() const noexcept;

  size_t size() const noexcept;

  size_t max_size() const noexcept { return kByteStringViewMaxSize; }

  absl::optional<absl::string_view> TryFlat() const noexcept
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::string_view GetFlat(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  bool Equals(ByteStringView rhs) const noexcept;

  int Compare(ByteStringView rhs) const noexcept;

  bool StartsWith(ByteStringView rhs) const noexcept;

  bool EndsWith(ByteStringView rhs) const noexcept;

  void RemovePrefix(size_t n);

  void RemoveSuffix(size_t n);

  std::string ToString() const;

  absl::Cord ToCord() const;

  absl::Nullable<google::protobuf::Arena*> GetArena() const noexcept;

  void HashValue(absl::HashState state) const;

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  Visit(Visitor&& visitor) const {
    switch (GetKind()) {
      case ByteStringViewKind::kString:
        return std::invoke(std::forward<Visitor>(visitor), GetString());
      case ByteStringViewKind::kCord:
        return std::invoke(std::forward<Visitor>(visitor),
                           static_cast<const absl::Cord&>(GetSubcord()));
    }
  }

 private:
  friend class ByteString;
  friend struct ByteStringViewTestFriend;

  constexpr ByteStringViewKind GetKind() const noexcept {
    return rep_.header.kind;
  }

  absl::string_view GetString() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringViewKind::kString);
    return absl::string_view(rep_.string.data, rep_.string.size);
  }

  absl::Nullable<google::protobuf::Arena*> GetStringArena() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringViewKind::kString);
    if ((rep_.string.owner & kMetadataOwnerBits) == kMetadataOwnerArenaBit) {
      return reinterpret_cast<google::protobuf::Arena*>(rep_.string.owner &
                                              kMetadataOwnerPointerMask);
    }
    return nullptr;
  }

  absl::Nullable<const ReferenceCount*> GetStringReferenceCount()
      const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringViewKind::kString);
    return GetStringReferenceCount(rep_.string);
  }

  static absl::Nullable<const ReferenceCount*> GetStringReferenceCount(
      const StringByteStringViewRep& rep) noexcept {
    if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerReferenceCountBit) {
      return reinterpret_cast<const ReferenceCount*>(rep.owner &
                                                     kMetadataOwnerPointerMask);
    }
    return nullptr;
  }

  uintptr_t GetStringOwner() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringViewKind::kString);
    return rep_.string.owner;
  }

  const absl::Cord& GetCord() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringViewKind::kCord);
    return *rep_.cord.data;
  }

  absl::Cord GetSubcord() const noexcept {
    ABSL_DCHECK_EQ(GetKind(), ByteStringViewKind::kCord);
    return GetCord().Subcord(rep_.cord.pos, rep_.cord.size);
  }

  ByteStringViewRep rep_;
};

inline bool operator==(const ByteString& lhs, const ByteString& rhs) noexcept {
  return lhs.Equals(rhs);
}

inline bool operator!=(const ByteString& lhs, const ByteString& rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline bool operator<(const ByteString& lhs, const ByteString& rhs) noexcept {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<=(const ByteString& lhs, const ByteString& rhs) noexcept {
  return lhs.Compare(rhs) <= 0;
}

inline bool operator>(const ByteString& lhs, const ByteString& rhs) noexcept {
  return lhs.Compare(rhs) > 0;
}

inline bool operator>=(const ByteString& lhs, const ByteString& rhs) noexcept {
  return lhs.Compare(rhs) >= 0;
}

inline bool ByteString::Equals(ByteStringView rhs) const noexcept {
  return ByteStringView(*this).Equals(rhs);
}

inline int ByteString::Compare(ByteStringView rhs) const noexcept {
  return ByteStringView(*this).Compare(rhs);
}

inline bool ByteString::StartsWith(ByteStringView rhs) const noexcept {
  return ByteStringView(*this).StartsWith(rhs);
}

inline bool ByteString::EndsWith(ByteStringView rhs) const noexcept {
  return ByteStringView(*this).EndsWith(rhs);
}

inline bool operator==(ByteStringView lhs, ByteStringView rhs) noexcept {
  return lhs.Equals(rhs);
}

inline bool operator!=(ByteStringView lhs, ByteStringView rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline bool operator<(ByteStringView lhs, ByteStringView rhs) noexcept {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<=(ByteStringView lhs, ByteStringView rhs) noexcept {
  return lhs.Compare(rhs) <= 0;
}

inline bool operator>(ByteStringView lhs, ByteStringView rhs) noexcept {
  return lhs.Compare(rhs) > 0;
}

inline bool operator>=(ByteStringView lhs, ByteStringView rhs) noexcept {
  return lhs.Compare(rhs) >= 0;
}

template <typename H>
H AbslHashValue(H state, ByteStringView byte_string_view) {
  byte_string_view.HashValue(absl::HashState::Create(&state));
  return state;
}

inline ByteString ByteString::Owned(Allocator<> allocator,
                                    ByteStringView other) {
  return ByteString(allocator, other);
}

inline ByteString::ByteString(ByteStringView other)
    : ByteString(NewDeleteAllocator(), other) {}

inline ByteString::ByteString(Allocator<> allocator, ByteStringView other)
    : ByteString(allocator) {
  CopyFrom(other);
}

inline ByteString& ByteString::operator=(ByteStringView other) {
  CopyFrom(other);
  return *this;
}

#undef CEL_COMMON_INTERNAL_BYTE_STRING_TRIVIAL_ABI

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_BYTE_STRING_H_
