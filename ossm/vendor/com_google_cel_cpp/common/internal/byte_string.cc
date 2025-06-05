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

#include "common/internal/byte_string.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/internal/metadata.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

namespace {

char* CopyCordToArray(const absl::Cord& cord, char* data) {
  for (auto chunk : cord.Chunks()) {
    std::memcpy(data, chunk.data(), chunk.size());
    data += chunk.size();
  }
  return data;
}

template <typename T>
T ConsumeAndDestroy(T& object) {
  T consumed = std::move(object);
  object.~T();  // NOLINT(bugprone-use-after-move)
  return consumed;
}

}  // namespace

ByteString::ByteString(Allocator<> allocator, absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  auto* arena = allocator.arena();
  if (string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, string);
  } else {
    SetMedium(arena, string);
  }
}

ByteString::ByteString(Allocator<> allocator, const std::string& string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  auto* arena = allocator.arena();
  if (string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, string);
  } else {
    SetMedium(arena, string);
  }
}

ByteString::ByteString(Allocator<> allocator, std::string&& string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  auto* arena = allocator.arena();
  if (string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, string);
  } else {
    SetMedium(arena, std::move(string));
  }
}

ByteString::ByteString(Allocator<> allocator, const absl::Cord& cord) {
  ABSL_DCHECK_LE(cord.size(), max_size());
  auto* arena = allocator.arena();
  if (cord.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, cord);
  } else if (arena != nullptr) {
    SetMedium(arena, cord);
  } else {
    SetLarge(cord);
  }
}

ByteString ByteString::Borrowed(Owner owner, absl::string_view string) {
  ABSL_DCHECK(owner != Owner::None()) << "Borrowing from Owner::None()";
  auto* arena = owner.arena();
  if (string.size() <= kSmallByteStringCapacity || arena != nullptr) {
    return ByteString(arena, string);
  }
  const auto* refcount = OwnerRelease(std::move(owner));
  // A nullptr refcount indicates somebody called us to borrow something that
  // has no owner. If this is the case, we fallback to assuming operator
  // new/delete and convert it to a reference count.
  if (refcount == nullptr) {
    std::tie(refcount, string) = MakeReferenceCountedString(string);
  }
  return ByteString(refcount, string);
}

ByteString ByteString::Borrowed(const Owner& owner, const absl::Cord& cord) {
  ABSL_DCHECK(owner != Owner::None()) << "Borrowing from Owner::None()";
  return ByteString(owner.arena(), cord);
}

ByteString::ByteString(absl::Nonnull<const ReferenceCount*> refcount,
                       absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  SetMedium(string, reinterpret_cast<uintptr_t>(refcount) |
                        kMetadataOwnerReferenceCountBit);
}

absl::Nullable<google::protobuf::Arena*> ByteString::GetArena() const noexcept {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmallArena();
    case ByteStringKind::kMedium:
      return GetMediumArena();
    case ByteStringKind::kLarge:
      return nullptr;
  }
}

bool ByteString::empty() const noexcept {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return rep_.small.size == 0;
    case ByteStringKind::kMedium:
      return rep_.medium.size == 0;
    case ByteStringKind::kLarge:
      return GetLarge().empty();
  }
}

size_t ByteString::size() const noexcept {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return rep_.small.size;
    case ByteStringKind::kMedium:
      return rep_.medium.size;
    case ByteStringKind::kLarge:
      return GetLarge().size();
  }
}

absl::string_view ByteString::Flatten() {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      return GetLarge().Flatten();
  }
}

absl::optional<absl::string_view> ByteString::TryFlat() const noexcept {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      return GetLarge().TryFlat();
  }
}

absl::string_view ByteString::GetFlat(std::string& scratch) const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge: {
      const auto& large = GetLarge();
      if (auto flat = large.TryFlat(); flat) {
        return *flat;
      }
      scratch = static_cast<std::string>(large);
      return scratch;
    }
  }
}

void ByteString::RemovePrefix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  if (n == 0) {
    return;
  }
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      std::memmove(rep_.small.data, rep_.small.data + n, rep_.small.size - n);
      rep_.small.size -= n;
      break;
    case ByteStringKind::kMedium:
      rep_.medium.data += n;
      rep_.medium.size -= n;
      if (rep_.medium.size <= kSmallByteStringCapacity) {
        const auto* refcount = GetMediumReferenceCount();
        SetSmall(GetMediumArena(), GetMedium());
        StrongUnref(refcount);
      }
      break;
    case ByteStringKind::kLarge: {
      auto& large = GetLarge();
      const auto large_size = large.size();
      const auto new_large_pos = n;
      const auto new_large_size = large_size - n;
      large = large.Subcord(new_large_pos, new_large_size);
      if (new_large_size <= kSmallByteStringCapacity) {
        auto large_copy = std::move(large);
        DestroyLarge();
        SetSmall(nullptr, large_copy);
      }
    } break;
  }
}

void ByteString::RemoveSuffix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  if (n == 0) {
    return;
  }
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      rep_.small.size -= n;
      break;
    case ByteStringKind::kMedium:
      rep_.medium.size -= n;
      if (rep_.medium.size <= kSmallByteStringCapacity) {
        const auto* refcount = GetMediumReferenceCount();
        SetSmall(GetMediumArena(), GetMedium());
        StrongUnref(refcount);
      }
      break;
    case ByteStringKind::kLarge: {
      auto& large = GetLarge();
      const auto large_size = large.size();
      const auto new_large_pos = 0;
      const auto new_large_size = large_size - n;
      large = large.Subcord(new_large_pos, new_large_size);
      if (new_large_size <= kSmallByteStringCapacity) {
        auto large_copy = std::move(large);
        DestroyLarge();
        SetSmall(nullptr, large_copy);
      }
    } break;
  }
}

std::string ByteString::ToString() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return std::string(GetSmall());
    case ByteStringKind::kMedium:
      return std::string(GetMedium());
    case ByteStringKind::kLarge:
      return static_cast<std::string>(GetLarge());
  }
}

namespace {

struct ReferenceCountReleaser {
  absl::Nonnull<const ReferenceCount*> refcount;

  void operator()() const noexcept { StrongUnref(*refcount); }
};

}  // namespace

absl::Cord ByteString::ToCord() const& {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return absl::Cord(GetSmall());
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        return absl::MakeCordFromExternal(GetMedium(),
                                          ReferenceCountReleaser{refcount});
      }
      return absl::Cord(GetMedium());
    }
    case ByteStringKind::kLarge:
      return GetLarge();
  }
}

absl::Cord ByteString::ToCord() && {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return absl::Cord(GetSmall());
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        auto medium = GetMedium();
        SetSmallEmpty(nullptr);
        return absl::MakeCordFromExternal(medium,
                                          ReferenceCountReleaser{refcount});
      }
      return absl::Cord(GetMedium());
    }
    case ByteStringKind::kLarge:
      return GetLarge();
  }
}

absl::Nullable<google::protobuf::Arena*> ByteString::GetMediumArena(
    const MediumByteStringRep& rep) noexcept {
  if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerArenaBit) {
    return reinterpret_cast<google::protobuf::Arena*>(rep.owner &
                                            kMetadataOwnerPointerMask);
  }
  return nullptr;
}

absl::Nullable<const ReferenceCount*> ByteString::GetMediumReferenceCount(
    const MediumByteStringRep& rep) noexcept {
  if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerReferenceCountBit) {
    return reinterpret_cast<const ReferenceCount*>(rep.owner &
                                                   kMetadataOwnerPointerMask);
  }
  return nullptr;
}

void ByteString::CopyFrom(const ByteString& other) {
  const auto kind = GetKind();
  const auto other_kind = other.GetKind();
  switch (kind) {
    case ByteStringKind::kSmall:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          CopyFromSmallSmall(other);
          break;
        case ByteStringKind::kMedium:
          CopyFromSmallMedium(other);
          break;
        case ByteStringKind::kLarge:
          CopyFromSmallLarge(other);
          break;
      }
      break;
    case ByteStringKind::kMedium:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          CopyFromMediumSmall(other);
          break;
        case ByteStringKind::kMedium:
          CopyFromMediumMedium(other);
          break;
        case ByteStringKind::kLarge:
          CopyFromMediumLarge(other);
          break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          CopyFromLargeSmall(other);
          break;
        case ByteStringKind::kMedium:
          CopyFromLargeMedium(other);
          break;
        case ByteStringKind::kLarge:
          CopyFromLargeLarge(other);
          break;
      }
      break;
  }
}

void ByteString::CopyFromSmallSmall(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kSmall);
  rep_.small.size = other.rep_.small.size;
  std::memcpy(rep_.small.data, other.rep_.small.data, rep_.small.size);
}

void ByteString::CopyFromSmallMedium(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kMedium);
  SetMedium(GetSmallArena(), other.GetMedium());
}

void ByteString::CopyFromSmallLarge(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kLarge);
  SetMediumOrLarge(GetSmallArena(), other.GetLarge());
}

void ByteString::CopyFromMediumSmall(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kSmall);
  auto* arena = GetMediumArena();
  if (arena == nullptr) {
    DestroyMedium();
  }
  SetSmall(arena, other.GetSmall());
}

void ByteString::CopyFromMediumMedium(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kMedium);
  auto* arena = GetMediumArena();
  auto* other_arena = other.GetMediumArena();
  if (arena == other_arena) {
    // No need to call `DestroyMedium`, we take care of the reference count
    // management directly.
    if (other_arena == nullptr) {
      StrongRef(other.GetMediumReferenceCount());
    }
    if (arena == nullptr) {
      StrongUnref(GetMediumReferenceCount());
    }
    SetMedium(other.GetMedium(), other.GetMediumOwner());
  } else {
    // Different allocator. This could be interesting.
    DestroyMedium();
    SetMedium(arena, other.GetMedium());
  }
}

void ByteString::CopyFromMediumLarge(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kLarge);
  auto* arena = GetMediumArena();
  if (arena == nullptr) {
    DestroyMedium();
    SetLarge(std::move(other.GetLarge()));
  } else {
    // No need to call `DestroyMedium`, it is guaranteed that we do not have a
    // reference count because `arena` is not `nullptr`.
    SetMedium(arena, other.GetLarge());
  }
}

void ByteString::CopyFromLargeSmall(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kSmall);
  DestroyLarge();
  SetSmall(nullptr, other.GetSmall());
}

void ByteString::CopyFromLargeMedium(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kMedium);
  const auto* refcount = other.GetMediumReferenceCount();
  if (refcount != nullptr) {
    StrongRef(*refcount);
    DestroyLarge();
    SetMedium(other.GetMedium(), other.GetMediumOwner());
  } else {
    GetLarge() = other.GetMedium();
  }
}

void ByteString::CopyFromLargeLarge(const ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kLarge);
  GetLarge() = std::move(other.GetLarge());
}

void ByteString::CopyFrom(ByteStringView other) {
  const auto kind = GetKind();
  const auto other_kind = other.GetKind();
  switch (kind) {
    case ByteStringKind::kSmall:
      switch (other_kind) {
        case ByteStringViewKind::kString:
          CopyFromSmallString(other);
          break;
        case ByteStringViewKind::kCord:
          CopyFromSmallCord(other);
          break;
      }
      break;
    case ByteStringKind::kMedium:
      switch (other_kind) {
        case ByteStringViewKind::kString:
          CopyFromMediumString(other);
          break;
        case ByteStringViewKind::kCord:
          CopyFromMediumCord(other);
          break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (other_kind) {
        case ByteStringViewKind::kString:
          CopyFromLargeString(other);
          break;
        case ByteStringViewKind::kCord:
          CopyFromLargeCord(other);
          break;
      }
      break;
  }
}

void ByteString::CopyFromSmallString(ByteStringView other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringViewKind::kString);
  auto* arena = GetSmallArena();
  const auto other_string = other.GetString();
  if (other_string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, other_string);
  } else {
    SetMedium(arena, other_string);
  }
}

void ByteString::CopyFromSmallCord(ByteStringView other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringViewKind::kCord);
  auto* arena = GetSmallArena();
  auto other_cord = other.GetSubcord();
  if (other_cord.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, other_cord);
  } else {
    SetMediumOrLarge(arena, std::move(other_cord));
  }
}

void ByteString::CopyFromMediumString(ByteStringView other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringViewKind::kString);
  auto* arena = GetMediumArena();
  const auto other_string = other.GetString();
  if (other_string.size() <= kSmallByteStringCapacity) {
    DestroyMedium();
    SetSmall(arena, other_string);
    return;
  }
  auto* other_arena = other.GetStringArena();
  if (arena == other_arena) {
    if (other_arena == nullptr) {
      StrongRef(other.GetStringReferenceCount());
    }
    if (arena == nullptr) {
      StrongUnref(GetMediumReferenceCount());
    }
    SetMedium(other_string, other.GetStringOwner());
  } else {
    DestroyMedium();
    SetMedium(arena, other_string);
  }
}

void ByteString::CopyFromMediumCord(ByteStringView other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringViewKind::kCord);
  auto* arena = GetMediumArena();
  auto other_cord = other.GetSubcord();
  DestroyMedium();
  if (other_cord.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, other_cord);
  } else {
    SetMediumOrLarge(arena, std::move(other_cord));
  }
}

void ByteString::CopyFromLargeString(ByteStringView other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringViewKind::kString);
  const auto other_string = other.GetString();
  if (other_string.size() <= kSmallByteStringCapacity) {
    DestroyLarge();
    SetSmall(nullptr, other_string);
    return;
  }
  auto* other_arena = other.GetStringArena();
  if (other_arena == nullptr) {
    const auto* refcount = other.GetStringReferenceCount();
    if (refcount != nullptr) {
      StrongRef(*refcount);
      DestroyLarge();
      SetMedium(other_string, other.GetStringOwner());
      return;
    }
  }
  GetLarge() = other_string;
}

void ByteString::CopyFromLargeCord(ByteStringView other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringViewKind::kCord);
  auto cord = other.GetSubcord();
  if (cord.size() <= kSmallByteStringCapacity) {
    DestroyLarge();
    SetSmall(nullptr, cord);
  } else {
    GetLarge() = std::move(cord);
  }
}

void ByteString::MoveFrom(ByteString& other) {
  const auto kind = GetKind();
  const auto other_kind = other.GetKind();
  switch (kind) {
    case ByteStringKind::kSmall:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          MoveFromSmallSmall(other);
          break;
        case ByteStringKind::kMedium:
          MoveFromSmallMedium(other);
          break;
        case ByteStringKind::kLarge:
          MoveFromSmallLarge(other);
          break;
      }
      break;
    case ByteStringKind::kMedium:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          MoveFromMediumSmall(other);
          break;
        case ByteStringKind::kMedium:
          MoveFromMediumMedium(other);
          break;
        case ByteStringKind::kLarge:
          MoveFromMediumLarge(other);
          break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          MoveFromLargeSmall(other);
          break;
        case ByteStringKind::kMedium:
          MoveFromLargeMedium(other);
          break;
        case ByteStringKind::kLarge:
          MoveFromLargeLarge(other);
          break;
      }
      break;
  }
}

void ByteString::MoveFromSmallSmall(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kSmall);
  rep_.small.size = other.rep_.small.size;
  std::memcpy(rep_.small.data, other.rep_.small.data, rep_.small.size);
  other.SetSmallEmpty(other.GetSmallArena());
}

void ByteString::MoveFromSmallMedium(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kMedium);
  auto* arena = GetSmallArena();
  auto* other_arena = other.GetMediumArena();
  if (arena == other_arena) {
    SetMedium(other.GetMedium(), other.GetMediumOwner());
  } else {
    SetMedium(arena, other.GetMedium());
    other.DestroyMedium();
  }
  other.SetSmallEmpty(other_arena);
}

void ByteString::MoveFromSmallLarge(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kLarge);
  auto* arena = GetSmallArena();
  if (arena == nullptr) {
    SetLarge(std::move(other.GetLarge()));
  } else {
    SetMediumOrLarge(arena, other.GetLarge());
  }
  other.DestroyLarge();
  other.SetSmallEmpty(nullptr);
}

void ByteString::MoveFromMediumSmall(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kSmall);
  auto* arena = GetMediumArena();
  auto* other_arena = other.GetSmallArena();
  if (arena == nullptr) {
    DestroyMedium();
  }
  SetSmall(arena, other.GetSmall());
  other.SetSmallEmpty(other_arena);
}

void ByteString::MoveFromMediumMedium(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kMedium);
  auto* arena = GetMediumArena();
  auto* other_arena = other.GetMediumArena();
  DestroyMedium();
  if (arena == other_arena) {
    SetMedium(other.GetMedium(), other.GetMediumOwner());
  } else {
    SetMedium(arena, other.GetMedium());
    other.DestroyMedium();
  }
  other.SetSmallEmpty(other_arena);
}

void ByteString::MoveFromMediumLarge(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kLarge);
  auto* arena = GetMediumArena();
  DestroyMedium();
  SetMediumOrLarge(arena, std::move(other.GetLarge()));
  other.DestroyLarge();
  other.SetSmallEmpty(nullptr);
}

void ByteString::MoveFromLargeSmall(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kSmall);
  auto* other_arena = other.GetSmallArena();
  DestroyLarge();
  SetSmall(nullptr, other.GetSmall());
  other.SetSmallEmpty(other_arena);
}

void ByteString::MoveFromLargeMedium(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kMedium);
  auto* other_arena = other.GetMediumArena();
  if (other_arena == nullptr) {
    DestroyLarge();
    SetMedium(other.GetMedium(), other.GetMediumOwner());
  } else {
    GetLarge() = other.GetMedium();
    other.DestroyMedium();
  }
  other.SetSmallEmpty(other_arena);
}

void ByteString::MoveFromLargeLarge(ByteString& other) {
  ABSL_DCHECK_EQ(GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(other.GetKind(), ByteStringKind::kLarge);
  GetLarge() = ConsumeAndDestroy(other.GetLarge());
  other.SetSmallEmpty(nullptr);
}

void ByteString::HashValue(absl::HashState state) const {
  Visit(absl::Overload(
      [&state](absl::string_view string) {
        absl::HashState::combine(std::move(state), string);
      },
      [&state](const absl::Cord& cord) {
        absl::HashState::combine(std::move(state), cord);
      }));
}

void ByteString::Swap(ByteString& other) {
  const auto kind = GetKind();
  const auto other_kind = other.GetKind();
  switch (kind) {
    case ByteStringKind::kSmall:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          SwapSmallSmall(*this, other);
          break;
        case ByteStringKind::kMedium:
          SwapSmallMedium(*this, other);
          break;
        case ByteStringKind::kLarge:
          SwapSmallLarge(*this, other);
          break;
      }
      break;
    case ByteStringKind::kMedium:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          SwapSmallMedium(other, *this);
          break;
        case ByteStringKind::kMedium:
          SwapMediumMedium(*this, other);
          break;
        case ByteStringKind::kLarge:
          SwapMediumLarge(*this, other);
          break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (other_kind) {
        case ByteStringKind::kSmall:
          SwapSmallLarge(other, *this);
          break;
        case ByteStringKind::kMedium:
          SwapMediumLarge(other, *this);
          break;
        case ByteStringKind::kLarge:
          SwapLargeLarge(*this, other);
          break;
      }
      break;
  }
}

void ByteString::Destroy() noexcept {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      break;
    case ByteStringKind::kMedium:
      DestroyMedium();
      break;
    case ByteStringKind::kLarge:
      DestroyLarge();
      break;
  }
}

void ByteString::SetSmallEmpty(absl::Nullable<google::protobuf::Arena*> arena) {
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = 0;
  rep_.small.arena = arena;
}

void ByteString::SetSmall(absl::Nullable<google::protobuf::Arena*> arena,
                          absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = string.size();
  rep_.small.arena = arena;
  std::memcpy(rep_.small.data, string.data(), rep_.small.size);
}

void ByteString::SetSmall(absl::Nullable<google::protobuf::Arena*> arena,
                          const absl::Cord& cord) {
  ABSL_DCHECK_LE(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = cord.size();
  rep_.small.arena = arena;
  (CopyCordToArray)(cord, rep_.small.data);
}

void ByteString::SetMedium(absl::Nullable<google::protobuf::Arena*> arena,
                           absl::string_view string) {
  ABSL_DCHECK_GT(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = string.size();
  if (arena != nullptr) {
    char* data = static_cast<char*>(
        arena->AllocateAligned(rep_.medium.size, alignof(char)));
    std::memcpy(data, string.data(), rep_.medium.size);
    rep_.medium.data = data;
    rep_.medium.owner =
        reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
  } else {
    auto pair = MakeReferenceCountedString(string);
    rep_.medium.data = pair.second.data();
    rep_.medium.owner = reinterpret_cast<uintptr_t>(pair.first) |
                        kMetadataOwnerReferenceCountBit;
  }
}

void ByteString::SetMedium(absl::Nullable<google::protobuf::Arena*> arena,
                           std::string&& string) {
  ABSL_DCHECK_GT(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = string.size();
  if (arena != nullptr) {
    auto* data = google::protobuf::Arena::Create<std::string>(arena, std::move(string));
    rep_.medium.data = data->data();
    rep_.medium.owner =
        reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
  } else {
    auto pair = MakeReferenceCountedString(std::move(string));
    rep_.medium.data = pair.second.data();
    rep_.medium.owner = reinterpret_cast<uintptr_t>(pair.first) |
                        kMetadataOwnerReferenceCountBit;
  }
}

void ByteString::SetMedium(absl::Nonnull<google::protobuf::Arena*> arena,
                           const absl::Cord& cord) {
  ABSL_DCHECK_GT(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = cord.size();
  char* data = static_cast<char*>(
      arena->AllocateAligned(rep_.medium.size, alignof(char)));
  (CopyCordToArray)(cord, data);
  rep_.medium.data = data;
  rep_.medium.owner =
      reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
}

void ByteString::SetMedium(absl::string_view string, uintptr_t owner) {
  ABSL_DCHECK_GT(string.size(), kSmallByteStringCapacity);
  ABSL_DCHECK_NE(owner, 0);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = string.size();
  rep_.medium.data = string.data();
  rep_.medium.owner = owner;
}

void ByteString::SetMediumOrLarge(absl::Nullable<google::protobuf::Arena*> arena,
                                  const absl::Cord& cord) {
  if (arena != nullptr) {
    SetMedium(arena, cord);
  } else {
    SetLarge(cord);
  }
}

void ByteString::SetMediumOrLarge(absl::Nullable<google::protobuf::Arena*> arena,
                                  absl::Cord&& cord) {
  if (arena != nullptr) {
    SetMedium(arena, cord);
  } else {
    SetLarge(std::move(cord));
  }
}

void ByteString::SetLarge(const absl::Cord& cord) {
  ABSL_DCHECK_GT(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kLarge;
  ::new (static_cast<void*>(&rep_.large.data[0])) absl::Cord(cord);
}

void ByteString::SetLarge(absl::Cord&& cord) {
  ABSL_DCHECK_GT(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kLarge;
  ::new (static_cast<void*>(&rep_.large.data[0])) absl::Cord(std::move(cord));
}

void ByteString::SwapSmallSmall(ByteString& lhs, ByteString& rhs) {
  using std::swap;
  ABSL_DCHECK_EQ(lhs.GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(rhs.GetKind(), ByteStringKind::kSmall);
  const auto size = lhs.rep_.small.size;
  lhs.rep_.small.size = rhs.rep_.small.size;
  rhs.rep_.small.size = size;
  swap(lhs.rep_.small.data, rhs.rep_.small.data);
}

void ByteString::SwapSmallMedium(ByteString& lhs, ByteString& rhs) {
  ABSL_DCHECK_EQ(lhs.GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(rhs.GetKind(), ByteStringKind::kMedium);
  auto* lhs_arena = lhs.GetSmallArena();
  auto* rhs_arena = rhs.GetMediumArena();
  if (lhs_arena == rhs_arena) {
    SmallByteStringRep lhs_rep = lhs.rep_.small;
    lhs.rep_.medium = rhs.rep_.medium;
    rhs.rep_.small = lhs_rep;
  } else {
    SmallByteStringRep small = lhs.rep_.small;
    lhs.SetMedium(lhs_arena, rhs.GetMedium());
    rhs.DestroyMedium();
    rhs.SetSmall(rhs_arena, GetSmall(small));
  }
}

void ByteString::SwapSmallLarge(ByteString& lhs, ByteString& rhs) {
  ABSL_DCHECK_EQ(lhs.GetKind(), ByteStringKind::kSmall);
  ABSL_DCHECK_EQ(rhs.GetKind(), ByteStringKind::kLarge);
  auto* lhs_arena = lhs.GetSmallArena();
  absl::Cord large = std::move(rhs.GetLarge());
  rhs.DestroyLarge();
  rhs.rep_.small = lhs.rep_.small;
  if (lhs_arena == nullptr) {
    lhs.SetLarge(std::move(large));
  } else {
    rhs.rep_.small.arena = nullptr;
    lhs.SetMedium(lhs_arena, large);
  }
}

void ByteString::SwapMediumMedium(ByteString& lhs, ByteString& rhs) {
  using std::swap;
  ABSL_DCHECK_EQ(lhs.GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(rhs.GetKind(), ByteStringKind::kMedium);
  auto* lhs_arena = lhs.GetMediumArena();
  auto* rhs_arena = rhs.GetMediumArena();
  if (lhs_arena == rhs_arena) {
    swap(lhs.rep_.medium, rhs.rep_.medium);
  } else {
    MediumByteStringRep medium = lhs.rep_.medium;
    lhs.SetMedium(lhs_arena, rhs.GetMedium());
    rhs.DestroyMedium();
    rhs.SetMedium(rhs_arena, GetMedium(medium));
    DestroyMedium(medium);
  }
}

void ByteString::SwapMediumLarge(ByteString& lhs, ByteString& rhs) {
  ABSL_DCHECK_EQ(lhs.GetKind(), ByteStringKind::kMedium);
  ABSL_DCHECK_EQ(rhs.GetKind(), ByteStringKind::kLarge);
  auto* lhs_arena = lhs.GetMediumArena();
  absl::Cord large = std::move(rhs.GetLarge());
  rhs.DestroyLarge();
  if (lhs_arena == nullptr) {
    rhs.rep_.medium = lhs.rep_.medium;
    lhs.SetLarge(std::move(large));
  } else {
    rhs.SetMedium(nullptr, lhs.GetMedium());
    lhs.SetMedium(lhs_arena, std::move(large));
  }
}

void ByteString::SwapLargeLarge(ByteString& lhs, ByteString& rhs) {
  using std::swap;
  ABSL_DCHECK_EQ(lhs.GetKind(), ByteStringKind::kLarge);
  ABSL_DCHECK_EQ(rhs.GetKind(), ByteStringKind::kLarge);
  swap(lhs.GetLarge(), rhs.GetLarge());
}

ByteStringView::ByteStringView(const ByteString& other) noexcept {
  switch (other.GetKind()) {
    case ByteStringKind::kSmall: {
      auto* other_arena = other.GetSmallArena();
      const auto string = other.GetSmall();
      rep_.header.kind = ByteStringViewKind::kString;
      rep_.string.size = string.size();
      rep_.string.data = string.data();
      if (other_arena != nullptr) {
        rep_.string.owner =
            reinterpret_cast<uintptr_t>(other_arena) | kMetadataOwnerArenaBit;
      } else {
        rep_.string.owner = 0;
      }
    } break;
    case ByteStringKind::kMedium: {
      const auto string = other.GetMedium();
      rep_.header.kind = ByteStringViewKind::kString;
      rep_.string.size = string.size();
      rep_.string.data = string.data();
      rep_.string.owner = other.GetMediumOwner();
    } break;
    case ByteStringKind::kLarge: {
      const auto& cord = other.GetLarge();
      rep_.header.kind = ByteStringViewKind::kCord;
      rep_.cord.size = cord.size();
      rep_.cord.data = &cord;
      rep_.cord.pos = 0;
    } break;
  }
}

bool ByteStringView::empty() const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return rep_.string.size == 0;
    case ByteStringViewKind::kCord:
      return rep_.cord.size == 0;
  }
}

size_t ByteStringView::size() const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return rep_.string.size;
    case ByteStringViewKind::kCord:
      return rep_.cord.size;
  }
}

absl::optional<absl::string_view> ByteStringView::TryFlat() const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return GetString();
    case ByteStringViewKind::kCord:
      if (auto flat = GetCord().TryFlat(); flat) {
        return flat->substr(rep_.cord.pos, rep_.cord.size);
      }
      return absl::nullopt;
  }
}

absl::string_view ByteStringView::GetFlat(std::string& scratch) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return GetString();
    case ByteStringViewKind::kCord: {
      if (auto flat = GetCord().TryFlat(); flat) {
        return flat->substr(rep_.cord.pos, rep_.cord.size);
      }
      scratch = static_cast<std::string>(GetSubcord());
      return scratch;
    }
  }
}

bool ByteStringView::Equals(ByteStringView rhs) const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetString() == rhs.GetString();
        case ByteStringViewKind::kCord:
          return GetString() == rhs.GetSubcord();
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord() == rhs.GetString();
        case ByteStringViewKind::kCord:
          return GetSubcord() == rhs.GetSubcord();
      }
  }
}

int ByteStringView::Compare(ByteStringView rhs) const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetString().compare(rhs.GetString());
        case ByteStringViewKind::kCord:
          return -rhs.GetSubcord().Compare(GetString());
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord().Compare(rhs.GetString());
        case ByteStringViewKind::kCord:
          return GetSubcord().Compare(rhs.GetSubcord());
      }
  }
}

bool ByteStringView::StartsWith(ByteStringView rhs) const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return absl::StartsWith(GetString(), rhs.GetString());
        case ByteStringViewKind::kCord: {
          const auto string = GetString();
          const auto& cord = rhs.GetSubcord();
          const auto cord_size = cord.size();
          return string.size() >= cord_size &&
                 string.substr(0, cord_size) == cord;
        }
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord().StartsWith(rhs.GetString());
        case ByteStringViewKind::kCord:
          return GetSubcord().StartsWith(rhs.GetSubcord());
      }
  }
}

bool ByteStringView::EndsWith(ByteStringView rhs) const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return absl::EndsWith(GetString(), rhs.GetString());
        case ByteStringViewKind::kCord: {
          const auto string = GetString();
          const auto& cord = rhs.GetSubcord();
          const auto string_size = string.size();
          const auto cord_size = cord.size();
          return string_size >= cord_size &&
                 string.substr(string_size - cord_size) == cord;
        }
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord().EndsWith(rhs.GetString());
        case ByteStringViewKind::kCord:
          return GetSubcord().EndsWith(rhs.GetSubcord());
      }
  }
}

void ByteStringView::RemovePrefix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      rep_.string.data += n;
      break;
    case ByteStringViewKind::kCord:
      rep_.cord.pos += n;
      break;
  }
  rep_.header.size -= n;
}

void ByteStringView::RemoveSuffix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  rep_.header.size -= n;
}

std::string ByteStringView::ToString() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return std::string(GetString());
    case ByteStringViewKind::kCord:
      return static_cast<std::string>(GetSubcord());
  }
}

absl::Cord ByteStringView::ToCord() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString: {
      const auto* refcount = GetStringReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        return absl::MakeCordFromExternal(GetString(),
                                          ReferenceCountReleaser{refcount});
      }
      return absl::Cord(GetString());
    }
    case ByteStringViewKind::kCord:
      return GetSubcord();
  }
}

absl::Nullable<google::protobuf::Arena*> ByteStringView::GetArena() const noexcept {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return GetStringArena();
    case ByteStringViewKind::kCord:
      return nullptr;
  }
}

void ByteStringView::HashValue(absl::HashState state) const {
  Visit(absl::Overload(
      [&state](absl::string_view string) {
        absl::HashState::combine(std::move(state), string);
      },
      [&state](const absl::Cord& cord) {
        absl::HashState::combine(std::move(state), cord);
      }));
}

}  // namespace cel::common_internal
