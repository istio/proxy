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
#include "absl/base/optimization.h"
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

ByteString ByteString::Concat(const ByteString& lhs, const ByteString& rhs,
                              google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(arena != nullptr);

  if (lhs.empty()) {
    return rhs;
  }
  if (rhs.empty()) {
    return lhs;
  }

  if (lhs.GetKind() == ByteStringKind::kLarge ||
      rhs.GetKind() == ByteStringKind::kLarge) {
    // If either the left or right are absl::Cord, use absl::Cord.
    absl::Cord result;
    result.Append(lhs.ToCord());
    result.Append(rhs.ToCord());
    return ByteString(std::move(result));
  }

  const size_t lhs_size = lhs.size();
  const size_t rhs_size = rhs.size();
  const size_t result_size = lhs_size + rhs_size;
  ByteString result;
  if (result_size <= kSmallByteStringCapacity) {
    // If the resulting string fits in inline storage, do it.
    result.rep_.small.size = result_size;
    result.rep_.small.arena = arena;
    lhs.CopyToArray(result.rep_.small.data);
    rhs.CopyToArray(result.rep_.small.data + lhs_size);
  } else {
    // Otherwise allocate on the arena.
    char* result_data =
        reinterpret_cast<char*>(arena->AllocateAligned(result_size));
    lhs.CopyToArray(result_data);
    rhs.CopyToArray(result_data + lhs_size);
    result.rep_.medium.data = result_data;
    result.rep_.medium.size = result_size;
    result.rep_.medium.owner =
        reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
    result.rep_.header.kind = ByteStringKind::kMedium;
  }
  return result;
}

ByteString::ByteString(Allocator<> allocator, absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), max_size());

  // Check for null data pointer in the string_view
  if (string.data() == nullptr) {
    // Handle null data by creating an empty ByteString
    SetSmallEmpty(allocator.arena());
    return;
  }
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

ByteString ByteString::Borrowed(Borrower borrower, absl::string_view string) {
  ABSL_DCHECK(borrower != Borrower::None()) << "Borrowing from Owner::None()";
  auto* arena = borrower.arena();
  if (string.size() <= kSmallByteStringCapacity || arena != nullptr) {
    return ByteString(arena, string);
  }
  const auto* refcount = BorrowerRelease(borrower);
  // A nullptr refcount indicates somebody called us to borrow something that
  // has no owner. If this is the case, we fallback to assuming operator
  // new/delete and convert it to a reference count.
  if (refcount == nullptr) {
    std::tie(refcount, string) = MakeReferenceCountedString(string);
  } else {
    StrongRef(*refcount);
  }
  return ByteString(refcount, string);
}

ByteString ByteString::Borrowed(Borrower borrower, const absl::Cord& cord) {
  ABSL_DCHECK(borrower != Borrower::None()) << "Borrowing from Owner::None()";
  return ByteString(borrower.arena(), cord);
}

ByteString::ByteString(const ReferenceCount* absl_nonnull refcount,
                       absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  SetMedium(string, reinterpret_cast<uintptr_t>(refcount) |
                        kMetadataOwnerReferenceCountBit);
}

google::protobuf::Arena* absl_nullable ByteString::GetArena() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmallArena();
    case ByteStringKind::kMedium:
      return GetMediumArena();
    case ByteStringKind::kLarge:
      return nullptr;
  }
}

bool ByteString::empty() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return rep_.small.size == 0;
    case ByteStringKind::kMedium:
      return rep_.medium.size == 0;
    case ByteStringKind::kLarge:
      return GetLarge().empty();
  }
}

size_t ByteString::size() const {
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

absl::optional<absl::string_view> ByteString::TryFlat() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      return GetLarge().TryFlat();
  }
}

bool ByteString::Equals(absl::string_view rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> bool { return lhs == rhs; },
      [&rhs](const absl::Cord& lhs) -> bool { return lhs == rhs; }));
}

bool ByteString::Equals(const absl::Cord& rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> bool { return lhs == rhs; },
      [&rhs](const absl::Cord& lhs) -> bool { return lhs == rhs; }));
}

int ByteString::Compare(absl::string_view rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> int { return lhs.compare(rhs); },
      [&rhs](const absl::Cord& lhs) -> int { return lhs.Compare(rhs); }));
}

int ByteString::Compare(const absl::Cord& rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> int { return -rhs.Compare(lhs); },
      [&rhs](const absl::Cord& lhs) -> int { return lhs.Compare(rhs); }));
}

bool ByteString::StartsWith(absl::string_view rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> bool {
        return absl::StartsWith(lhs, rhs);
      },
      [&rhs](const absl::Cord& lhs) -> bool { return lhs.StartsWith(rhs); }));
}

bool ByteString::StartsWith(const absl::Cord& rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> bool {
        return lhs.size() >= rhs.size() && lhs.substr(0, rhs.size()) == rhs;
      },
      [&rhs](const absl::Cord& lhs) -> bool { return lhs.StartsWith(rhs); }));
}

bool ByteString::EndsWith(absl::string_view rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> bool {
        return absl::EndsWith(lhs, rhs);
      },
      [&rhs](const absl::Cord& lhs) -> bool { return lhs.EndsWith(rhs); }));
}

bool ByteString::EndsWith(const absl::Cord& rhs) const {
  return Visit(absl::Overload(
      [&rhs](absl::string_view lhs) -> bool {
        return lhs.size() >= rhs.size() &&
               lhs.substr(lhs.size() - rhs.size()) == rhs;
      },
      [&rhs](const absl::Cord& lhs) -> bool { return lhs.EndsWith(rhs); }));
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

void ByteString::CopyToArray(char* absl_nonnull out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall: {
      absl::string_view small = GetSmall();
      std::memcpy(out, small.data(), small.size());
    } break;
    case ByteStringKind::kMedium: {
      absl::string_view medium = GetMedium();
      std::memcpy(out, medium.data(), medium.size());
    } break;
    case ByteStringKind::kLarge: {
      const absl::Cord& large = GetLarge();
      (CopyCordToArray)(large, out);
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

void ByteString::CopyToString(std::string* absl_nonnull out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      out->assign(GetSmall());
      break;
    case ByteStringKind::kMedium:
      out->assign(GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::CopyCordToString(GetLarge(), out);
      break;
  }
}

void ByteString::AppendToString(std::string* absl_nonnull out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      out->append(GetSmall());
      break;
    case ByteStringKind::kMedium:
      out->append(GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::AppendCordToString(GetLarge(), out);
      break;
  }
}

namespace {

struct ReferenceCountReleaser {
  const ReferenceCount* absl_nonnull refcount;

  void operator()() const { StrongUnref(*refcount); }
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

void ByteString::CopyToCord(absl::Cord* absl_nonnull out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      *out = absl::Cord(GetSmall());
      break;
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        *out = absl::MakeCordFromExternal(GetMedium(),
                                          ReferenceCountReleaser{refcount});
      } else {
        *out = absl::Cord(GetMedium());
      }
    } break;
    case ByteStringKind::kLarge:
      *out = GetLarge();
      break;
  }
}

void ByteString::AppendToCord(absl::Cord* absl_nonnull out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      out->Append(GetSmall());
      break;
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        out->Append(absl::MakeCordFromExternal(
            GetMedium(), ReferenceCountReleaser{refcount}));
      } else {
        out->Append(GetMedium());
      }
    } break;
    case ByteStringKind::kLarge:
      out->Append(GetLarge());
      break;
  }
}

absl::string_view ByteString::ToStringView(
    std::string* absl_nonnull scratch) const {
  ABSL_DCHECK(scratch != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      if (auto flat = GetLarge().TryFlat(); flat) {
        return *flat;
      }
      absl::CopyCordToString(GetLarge(), scratch);
      return absl::string_view(*scratch);
  }
}

absl::string_view ByteString::AsStringView() const {
  const ByteStringKind kind = GetKind();
  ABSL_CHECK(kind == ByteStringKind::kSmall ||  // Crash OK
             kind == ByteStringKind::kMedium);
  switch (kind) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      ABSL_UNREACHABLE();
  }
}

google::protobuf::Arena* absl_nullable ByteString::GetMediumArena(
    const MediumByteStringRep& rep) {
  if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerArenaBit) {
    return reinterpret_cast<google::protobuf::Arena*>(rep.owner &
                                            kMetadataOwnerPointerMask);
  }
  return nullptr;
}

const ReferenceCount* absl_nullable ByteString::GetMediumReferenceCount(
    const MediumByteStringRep& rep) {
  if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerReferenceCountBit) {
    return reinterpret_cast<const ReferenceCount*>(rep.owner &
                                                   kMetadataOwnerPointerMask);
  }
  return nullptr;
}

void ByteString::Construct(const ByteString& other,
                           absl::optional<Allocator<>> allocator) {
  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      rep_.small = other.rep_.small;
      if (allocator.has_value()) {
        rep_.small.arena = allocator->arena();
      }
      break;
    case ByteStringKind::kMedium:
      if (allocator.has_value() &&
          allocator->arena() != other.GetMediumArena()) {
        SetMedium(allocator->arena(), other.GetMedium());
      } else {
        rep_.medium = other.rep_.medium;
        StrongRef(GetMediumReferenceCount());
      }
      break;
    case ByteStringKind::kLarge:
      if (allocator.has_value() && allocator->arena() != nullptr) {
        SetMedium(allocator->arena(), other.GetLarge());
      } else {
        SetLarge(other.GetLarge());
      }
      break;
  }
}

void ByteString::Construct(ByteString& other,
                           absl::optional<Allocator<>> allocator) {
  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      rep_.small = other.rep_.small;
      if (allocator.has_value()) {
        rep_.small.arena = allocator->arena();
      }
      break;
    case ByteStringKind::kMedium:
      if (allocator.has_value() &&
          allocator->arena() != other.GetMediumArena()) {
        SetMedium(allocator->arena(), other.GetMedium());
      } else {
        rep_.medium = other.rep_.medium;
        other.rep_.medium.owner = 0;
      }
      break;
    case ByteStringKind::kLarge:
      if (allocator.has_value() && allocator->arena() != nullptr) {
        SetMedium(allocator->arena(), other.GetLarge());
      } else {
        SetLarge(std::move(other.GetLarge()));
      }
      break;
  }
}

void ByteString::CopyFrom(const ByteString& other) {
  ABSL_DCHECK_NE(&other, this);

  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
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
      rep_.small = other.rep_.small;
      break;
    case ByteStringKind::kMedium:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          rep_.medium = other.rep_.medium;
          StrongRef(GetMediumReferenceCount());
          break;
        case ByteStringKind::kMedium:
          StrongRef(other.GetMediumReferenceCount());
          DestroyMedium();
          rep_.medium = other.rep_.medium;
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          rep_.medium = other.rep_.medium;
          StrongRef(GetMediumReferenceCount());
          break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          SetLarge(other.GetLarge());
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          SetLarge(other.GetLarge());
          break;
        case ByteStringKind::kLarge:
          GetLarge() = other.GetLarge();
          break;
      }
      break;
  }
}

void ByteString::MoveFrom(ByteString& other) {
  ABSL_DCHECK_NE(&other, this);

  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
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
      rep_.small = other.rep_.small;
      break;
    case ByteStringKind::kMedium:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          rep_.medium = other.rep_.medium;
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          rep_.medium = other.rep_.medium;
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          rep_.medium = other.rep_.medium;
          break;
      }
      other.rep_.medium.owner = 0;
      break;
    case ByteStringKind::kLarge:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          SetLarge(std::move(other.GetLarge()));
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          SetLarge(std::move(other.GetLarge()));
          break;
        case ByteStringKind::kLarge:
          GetLarge() = std::move(other.GetLarge());
          break;
      }
      break;
  }
}

ByteString ByteString::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return ByteString(arena, GetSmall());
    case ByteStringKind::kMedium: {
      google::protobuf::Arena* absl_nullable other_arena = GetMediumArena();
      if (arena != nullptr) {
        if (arena == other_arena) {
          return *this;
        }
        return ByteString(arena, GetMedium());
      }
      if (other_arena != nullptr) {
        return ByteString(arena, GetMedium());
      }
      return *this;
    }
    case ByteStringKind::kLarge:
      return ByteString(arena, GetLarge());
  }
}

void ByteString::HashValue(absl::HashState state) const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      absl::HashState::combine(std::move(state), GetSmall());
      break;
    case ByteStringKind::kMedium:
      absl::HashState::combine(std::move(state), GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::HashState::combine(std::move(state), GetLarge());
      break;
  }
}

void ByteString::Swap(ByteString& other) {
  ABSL_DCHECK_NE(&other, this);
  using std::swap;

  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          // small <=> small
          swap(rep_.small, other.rep_.small);
          break;
        case ByteStringKind::kMedium:
          // medium <=> small
          swap(rep_, other.rep_);
          break;
        case ByteStringKind::kLarge: {
          absl::Cord cord = std::move(GetLarge());
          DestroyLarge();
          rep_ = other.rep_;
          other.SetLarge(std::move(cord));
        } break;
      }
      break;
    case ByteStringKind::kMedium:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          swap(rep_, other.rep_);
          break;
        case ByteStringKind::kMedium:
          swap(rep_.medium, other.rep_.medium);
          break;
        case ByteStringKind::kLarge: {
          absl::Cord cord = std::move(GetLarge());
          DestroyLarge();
          rep_ = other.rep_;
          other.SetLarge(std::move(cord));
        } break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (GetKind()) {
        case ByteStringKind::kSmall: {
          absl::Cord cord = std::move(other.GetLarge());
          other.DestroyLarge();
          other.rep_.small = rep_.small;
          SetLarge(std::move(cord));
        } break;
        case ByteStringKind::kMedium: {
          absl::Cord cord = std::move(other.GetLarge());
          other.DestroyLarge();
          other.rep_.medium = rep_.medium;
          SetLarge(std::move(cord));
        } break;
        case ByteStringKind::kLarge:
          swap(GetLarge(), other.GetLarge());
          break;
      }
      break;
  }
}

void ByteString::Destroy() {
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

void ByteString::SetSmall(google::protobuf::Arena* absl_nullable arena,
                          absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = string.size();
  rep_.small.arena = arena;
  std::memcpy(rep_.small.data, string.data(), rep_.small.size);
}

void ByteString::SetSmall(google::protobuf::Arena* absl_nullable arena,
                          const absl::Cord& cord) {
  ABSL_DCHECK_LE(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = cord.size();
  rep_.small.arena = arena;
  (CopyCordToArray)(cord, rep_.small.data);
}

void ByteString::SetMedium(google::protobuf::Arena* absl_nullable arena,
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

void ByteString::SetMedium(google::protobuf::Arena* absl_nullable arena,
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

void ByteString::SetMedium(google::protobuf::Arena* absl_nonnull arena,
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

absl::string_view LegacyByteString(const ByteString& string, bool stable,
                                   google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(arena != nullptr);
  if (string.empty()) {
    return absl::string_view();
  }
  const ByteStringKind kind = string.GetKind();
  if (kind == ByteStringKind::kMedium && string.GetMediumArena() == arena) {
    google::protobuf::Arena* absl_nullable other_arena = string.GetMediumArena();
    if (other_arena == arena || other_arena == nullptr) {
      // Legacy values do not preserve arena. For speed, we assume the arena is
      // compatible.
      return string.GetMedium();
    }
  }
  if (stable && kind == ByteStringKind::kSmall) {
    return string.GetSmall();
  }
  std::string* absl_nonnull result = google::protobuf::Arena::Create<std::string>(arena);
  switch (kind) {
    case ByteStringKind::kSmall:
      result->assign(string.GetSmall());
      break;
    case ByteStringKind::kMedium:
      result->assign(string.GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::CopyCordToString(string.GetLarge(), result);
      break;
  }
  return absl::string_view(*result);
}

}  // namespace cel::common_internal
