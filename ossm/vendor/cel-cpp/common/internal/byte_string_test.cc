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

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/hash/hash.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

struct ByteStringTestFriend {
  static ByteStringKind GetKind(const ByteString& byte_string) {
    return byte_string.GetKind();
  }
};

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::TestWithParam;

TEST(ByteStringKind, Ostream) {
  {
    std::ostringstream out;
    out << ByteStringKind::kSmall;
    EXPECT_EQ(out.str(), "SMALL");
  }
  {
    std::ostringstream out;
    out << ByteStringKind::kMedium;
    EXPECT_EQ(out.str(), "MEDIUM");
  }
  {
    std::ostringstream out;
    out << ByteStringKind::kLarge;
    EXPECT_EQ(out.str(), "LARGE");
  }
}

class ByteStringTest : public TestWithParam<AllocatorKind>,
                       public ByteStringTestFriend {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case AllocatorKind::kNewDelete:
        return NewDeleteAllocator<>{};
      case AllocatorKind::kArena:
        return ArenaAllocator<>(&arena_);
    }
  }

 private:
  google::protobuf::Arena arena_;
};

absl::string_view GetSmallStringView() {
  static constexpr absl::string_view small = "A small string!";
  return small.substr(0, std::min(kSmallByteStringCapacity, small.size()));
}

std::string GetSmallString() { return std::string(GetSmallStringView()); }

absl::Cord GetSmallCord() {
  static const absl::NoDestructor<absl::Cord> small(GetSmallStringView());
  return *small;
}

absl::string_view GetMediumStringView() {
  static constexpr absl::string_view medium =
      "A string that is too large for the small string optimization!";
  return medium;
}

std::string GetMediumString() { return std::string(GetMediumStringView()); }

const absl::Cord& GetMediumOrLargeCord() {
  static const absl::NoDestructor<absl::Cord> medium_or_large(
      GetMediumStringView());
  return *medium_or_large;
}

const absl::Cord& GetMediumOrLargeFragmentedCord() {
  static const absl::NoDestructor<absl::Cord> medium_or_large(
      absl::MakeFragmentedCord(
          {GetMediumStringView().substr(0, kSmallByteStringCapacity),
           GetMediumStringView().substr(kSmallByteStringCapacity)}));
  return *medium_or_large;
}

TEST_P(ByteStringTest, Default) {
  ByteString byte_string = ByteString(GetAllocator(), "");
  EXPECT_THAT(byte_string, SizeIs(0));
  EXPECT_THAT(byte_string, IsEmpty());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, ConstructSmallCString) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallString().c_str());
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumCString) {
  ByteString byte_string =
      ByteString(GetAllocator(), GetMediumString().c_str());
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructSmallRValueString) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallString());
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructSmallLValueString) {
  ByteString byte_string = ByteString(
      GetAllocator(), static_cast<const std::string&>(GetSmallString()));
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumRValueString) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumString());
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumLValueString) {
  ByteString byte_string = ByteString(
      GetAllocator(), static_cast<const std::string&>(GetMediumString()));
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructSmallCord) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallCord());
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumOrLargeCord) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  if (GetAllocator().arena() == nullptr) {
    EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
  } else {
    EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  }
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST(ByteStringTest, BorrowedUnownedString) {
#ifdef NDEBUG
  ByteString byte_string = ByteString(Owner::None(), GetMediumStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumStringView());
#else
  EXPECT_DEBUG_DEATH(
      static_cast<void>(ByteString(Owner::None(), GetMediumStringView())),
      ::testing::_);
#endif
}

TEST(ByteStringTest, BorrowedUnownedCord) {
#ifdef NDEBUG
  ByteString byte_string = ByteString(Owner::None(), GetMediumOrLargeCord());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumOrLargeCord());
#else
  EXPECT_DEBUG_DEATH(
      static_cast<void>(ByteString(Owner::None(), GetMediumOrLargeCord())),
      ::testing::_);
#endif
}

TEST(ByteStringTest, BorrowedReferenceCountSmallString) {
  auto* refcount = new ReferenceCounted();
  Owner owner = Owner::ReferenceCount(refcount);
  StrongUnref(refcount);
  ByteString byte_string = ByteString(owner, GetSmallStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetSmallStringView());
}

TEST(ByteStringTest, BorrowedReferenceCountMediumString) {
  auto* refcount = new ReferenceCounted();
  Owner owner = Owner::ReferenceCount(refcount);
  StrongUnref(refcount);
  ByteString byte_string = ByteString(owner, GetMediumStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumStringView());
}

TEST(ByteStringTest, BorrowedArenaSmallString) {
  google::protobuf::Arena arena;
  ByteString byte_string =
      ByteString(Owner::Arena(&arena), GetSmallStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), &arena);
  EXPECT_EQ(byte_string, GetSmallStringView());
}

TEST(ByteStringTest, BorrowedArenaMediumString) {
  google::protobuf::Arena arena;
  ByteString byte_string =
      ByteString(Owner::Arena(&arena), GetMediumStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), &arena);
  EXPECT_EQ(byte_string, GetMediumStringView());
}

TEST(ByteStringTest, BorrowedReferenceCountCord) {
  auto* refcount = new ReferenceCounted();
  Owner owner = Owner::ReferenceCount(refcount);
  StrongUnref(refcount);
  ByteString byte_string = ByteString(owner, GetMediumOrLargeCord());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumOrLargeCord());
}

TEST(ByteStringTest, BorrowedArenaCord) {
  google::protobuf::Arena arena;
  Owner owner = Owner::Arena(&arena);
  ByteString byte_string = ByteString(owner, GetMediumOrLargeCord());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), &arena);
  EXPECT_EQ(byte_string, GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, CopyConstruct) {
  ByteString small_byte_string =
      ByteString(GetAllocator(), GetSmallStringView());
  ByteString medium_byte_string =
      ByteString(GetAllocator(), GetMediumStringView());
  ByteString large_byte_string =
      ByteString(GetAllocator(), GetMediumOrLargeCord());

  EXPECT_EQ(ByteString(NewDeleteAllocator(), small_byte_string),
            small_byte_string);
  EXPECT_EQ(ByteString(NewDeleteAllocator(), medium_byte_string),
            medium_byte_string);
  EXPECT_EQ(ByteString(NewDeleteAllocator(), large_byte_string),
            large_byte_string);

  google::protobuf::Arena arena;
  EXPECT_EQ(ByteString(ArenaAllocator(&arena), small_byte_string),
            small_byte_string);
  EXPECT_EQ(ByteString(ArenaAllocator(&arena), medium_byte_string),
            medium_byte_string);
  EXPECT_EQ(ByteString(ArenaAllocator(&arena), large_byte_string),
            large_byte_string);

  EXPECT_EQ(ByteString(GetAllocator(), small_byte_string), small_byte_string);
  EXPECT_EQ(ByteString(GetAllocator(), medium_byte_string), medium_byte_string);
  EXPECT_EQ(ByteString(GetAllocator(), large_byte_string), large_byte_string);

  EXPECT_EQ(ByteString(small_byte_string), small_byte_string);
  EXPECT_EQ(ByteString(medium_byte_string), medium_byte_string);
  EXPECT_EQ(ByteString(large_byte_string), large_byte_string);
}

TEST_P(ByteStringTest, MoveConstruct) {
  const auto& small_byte_string = [this]() {
    return ByteString(GetAllocator(), GetSmallStringView());
  };
  const auto& medium_byte_string = [this]() {
    return ByteString(GetAllocator(), GetMediumStringView());
  };
  const auto& large_byte_string = [this]() {
    return ByteString(GetAllocator(), GetMediumOrLargeCord());
  };

  EXPECT_EQ(ByteString(NewDeleteAllocator(), small_byte_string()),
            small_byte_string());
  EXPECT_EQ(ByteString(NewDeleteAllocator(), medium_byte_string()),
            medium_byte_string());
  EXPECT_EQ(ByteString(NewDeleteAllocator(), large_byte_string()),
            large_byte_string());

  google::protobuf::Arena arena;
  EXPECT_EQ(ByteString(ArenaAllocator(&arena), small_byte_string()),
            small_byte_string());
  EXPECT_EQ(ByteString(ArenaAllocator(&arena), medium_byte_string()),
            medium_byte_string());
  EXPECT_EQ(ByteString(ArenaAllocator(&arena), large_byte_string()),
            large_byte_string());

  EXPECT_EQ(ByteString(GetAllocator(), small_byte_string()),
            small_byte_string());
  EXPECT_EQ(ByteString(GetAllocator(), medium_byte_string()),
            medium_byte_string());
  EXPECT_EQ(ByteString(GetAllocator(), large_byte_string()),
            large_byte_string());

  EXPECT_EQ(ByteString(small_byte_string()), small_byte_string());
  EXPECT_EQ(ByteString(medium_byte_string()), medium_byte_string());
  EXPECT_EQ(ByteString(large_byte_string()), large_byte_string());
}

TEST_P(ByteStringTest, CopyFromByteString) {
  ByteString small_byte_string =
      ByteString(GetAllocator(), GetSmallStringView());
  ByteString medium_byte_string =
      ByteString(GetAllocator(), GetMediumStringView());
  ByteString large_byte_string =
      ByteString(GetAllocator(), GetMediumOrLargeCord());

  ByteString new_delete_byte_string(NewDeleteAllocator<>{});
  // Small <= Small
  new_delete_byte_string = small_byte_string;
  EXPECT_EQ(new_delete_byte_string, small_byte_string);
  // Small <= Medium
  new_delete_byte_string = medium_byte_string;
  EXPECT_EQ(new_delete_byte_string, medium_byte_string);
  // Medium <= Medium
  new_delete_byte_string = medium_byte_string;
  EXPECT_EQ(new_delete_byte_string, medium_byte_string);
  // Medium <= Large
  new_delete_byte_string = large_byte_string;
  EXPECT_EQ(new_delete_byte_string, large_byte_string);
  // Large <= Large
  new_delete_byte_string = large_byte_string;
  EXPECT_EQ(new_delete_byte_string, large_byte_string);
  // Large <= Small
  new_delete_byte_string = small_byte_string;
  EXPECT_EQ(new_delete_byte_string, small_byte_string);
  // Small <= Large
  new_delete_byte_string = large_byte_string;
  EXPECT_EQ(new_delete_byte_string, large_byte_string);
  // Large <= Medium
  new_delete_byte_string = medium_byte_string;
  EXPECT_EQ(new_delete_byte_string, medium_byte_string);
  // Medium <= Small
  new_delete_byte_string = small_byte_string;
  EXPECT_EQ(new_delete_byte_string, small_byte_string);

  google::protobuf::Arena arena;
  ByteString arena_byte_string(ArenaAllocator<>{&arena});
  // Small <= Small
  arena_byte_string = small_byte_string;
  EXPECT_EQ(arena_byte_string, small_byte_string);
  // Small <= Medium
  arena_byte_string = medium_byte_string;
  EXPECT_EQ(arena_byte_string, medium_byte_string);
  // Medium <= Medium
  arena_byte_string = medium_byte_string;
  EXPECT_EQ(arena_byte_string, medium_byte_string);
  // Medium <= Large
  arena_byte_string = large_byte_string;
  EXPECT_EQ(arena_byte_string, large_byte_string);
  // Large <= Large
  arena_byte_string = large_byte_string;
  EXPECT_EQ(arena_byte_string, large_byte_string);
  // Large <= Small
  arena_byte_string = small_byte_string;
  EXPECT_EQ(arena_byte_string, small_byte_string);
  // Small <= Large
  arena_byte_string = large_byte_string;
  EXPECT_EQ(arena_byte_string, large_byte_string);
  // Large <= Medium
  arena_byte_string = medium_byte_string;
  EXPECT_EQ(arena_byte_string, medium_byte_string);
  // Medium <= Small
  arena_byte_string = small_byte_string;
  EXPECT_EQ(arena_byte_string, small_byte_string);

  ByteString allocator_byte_string(GetAllocator());
  // Small <= Small
  allocator_byte_string = small_byte_string;
  EXPECT_EQ(allocator_byte_string, small_byte_string);
  // Small <= Medium
  allocator_byte_string = medium_byte_string;
  EXPECT_EQ(allocator_byte_string, medium_byte_string);
  // Medium <= Medium
  allocator_byte_string = medium_byte_string;
  EXPECT_EQ(allocator_byte_string, medium_byte_string);
  // Medium <= Large
  allocator_byte_string = large_byte_string;
  EXPECT_EQ(allocator_byte_string, large_byte_string);
  // Large <= Large
  allocator_byte_string = large_byte_string;
  EXPECT_EQ(allocator_byte_string, large_byte_string);
  // Large <= Small
  allocator_byte_string = small_byte_string;
  EXPECT_EQ(allocator_byte_string, small_byte_string);
  // Small <= Large
  allocator_byte_string = large_byte_string;
  EXPECT_EQ(allocator_byte_string, large_byte_string);
  // Large <= Medium
  allocator_byte_string = medium_byte_string;
  EXPECT_EQ(allocator_byte_string, medium_byte_string);
  // Medium <= Small
  allocator_byte_string = small_byte_string;
  EXPECT_EQ(allocator_byte_string, small_byte_string);

  // Miscellaneous cases not covered above.
  // Large <= Medium Arena String
  ByteString large_new_delete_byte_string(NewDeleteAllocator<>{},
                                          GetMediumOrLargeCord());
  ByteString medium_arena_byte_string(ArenaAllocator<>{&arena},
                                      GetMediumStringView());
  large_new_delete_byte_string = medium_arena_byte_string;
  EXPECT_EQ(large_new_delete_byte_string, medium_arena_byte_string);
}

TEST_P(ByteStringTest, MoveFrom) {
  const auto& small_byte_string = [this]() {
    return ByteString(GetAllocator(), GetSmallStringView());
  };
  const auto& medium_byte_string = [this]() {
    return ByteString(GetAllocator(), GetMediumStringView());
  };
  const auto& large_byte_string = [this]() {
    return ByteString(GetAllocator(), GetMediumOrLargeCord());
  };

  ByteString new_delete_byte_string(NewDeleteAllocator<>{});
  // Small <= Small
  new_delete_byte_string = small_byte_string();
  EXPECT_EQ(new_delete_byte_string, small_byte_string());
  // Small <= Medium
  new_delete_byte_string = medium_byte_string();
  EXPECT_EQ(new_delete_byte_string, medium_byte_string());
  // Medium <= Medium
  new_delete_byte_string = medium_byte_string();
  EXPECT_EQ(new_delete_byte_string, medium_byte_string());
  // Medium <= Large
  new_delete_byte_string = large_byte_string();
  EXPECT_EQ(new_delete_byte_string, large_byte_string());
  // Large <= Large
  new_delete_byte_string = large_byte_string();
  EXPECT_EQ(new_delete_byte_string, large_byte_string());
  // Large <= Small
  new_delete_byte_string = small_byte_string();
  EXPECT_EQ(new_delete_byte_string, small_byte_string());
  // Small <= Large
  new_delete_byte_string = large_byte_string();
  EXPECT_EQ(new_delete_byte_string, large_byte_string());
  // Large <= Medium
  new_delete_byte_string = medium_byte_string();
  EXPECT_EQ(new_delete_byte_string, medium_byte_string());
  // Medium <= Small
  new_delete_byte_string = small_byte_string();
  EXPECT_EQ(new_delete_byte_string, small_byte_string());

  google::protobuf::Arena arena;
  ByteString arena_byte_string(ArenaAllocator<>{&arena});
  // Small <= Small
  arena_byte_string = small_byte_string();
  EXPECT_EQ(arena_byte_string, small_byte_string());
  // Small <= Medium
  arena_byte_string = medium_byte_string();
  EXPECT_EQ(arena_byte_string, medium_byte_string());
  // Medium <= Medium
  arena_byte_string = medium_byte_string();
  EXPECT_EQ(arena_byte_string, medium_byte_string());
  // Medium <= Large
  arena_byte_string = large_byte_string();
  EXPECT_EQ(arena_byte_string, large_byte_string());
  // Large <= Large
  arena_byte_string = large_byte_string();
  EXPECT_EQ(arena_byte_string, large_byte_string());
  // Large <= Small
  arena_byte_string = small_byte_string();
  EXPECT_EQ(arena_byte_string, small_byte_string());
  // Small <= Large
  arena_byte_string = large_byte_string();
  EXPECT_EQ(arena_byte_string, large_byte_string());
  // Large <= Medium
  arena_byte_string = medium_byte_string();
  EXPECT_EQ(arena_byte_string, medium_byte_string());
  // Medium <= Small
  arena_byte_string = small_byte_string();
  EXPECT_EQ(arena_byte_string, small_byte_string());

  ByteString allocator_byte_string(GetAllocator());
  // Small <= Small
  allocator_byte_string = small_byte_string();
  EXPECT_EQ(allocator_byte_string, small_byte_string());
  // Small <= Medium
  allocator_byte_string = medium_byte_string();
  EXPECT_EQ(allocator_byte_string, medium_byte_string());
  // Medium <= Medium
  allocator_byte_string = medium_byte_string();
  EXPECT_EQ(allocator_byte_string, medium_byte_string());
  // Medium <= Large
  allocator_byte_string = large_byte_string();
  EXPECT_EQ(allocator_byte_string, large_byte_string());
  // Large <= Large
  allocator_byte_string = large_byte_string();
  EXPECT_EQ(allocator_byte_string, large_byte_string());
  // Large <= Small
  allocator_byte_string = small_byte_string();
  EXPECT_EQ(allocator_byte_string, small_byte_string());
  // Small <= Large
  allocator_byte_string = large_byte_string();
  EXPECT_EQ(allocator_byte_string, large_byte_string());
  // Large <= Medium
  allocator_byte_string = medium_byte_string();
  EXPECT_EQ(allocator_byte_string, medium_byte_string());
  // Medium <= Small
  allocator_byte_string = small_byte_string();
  EXPECT_EQ(allocator_byte_string, small_byte_string());

  // Miscellaneous cases not covered above.
  // Large <= Medium Arena String
  ByteString large_new_delete_byte_string(NewDeleteAllocator<>{},
                                          GetMediumOrLargeCord());
  ByteString medium_arena_byte_string(ArenaAllocator<>{&arena},
                                      GetMediumStringView());
  large_new_delete_byte_string = std::move(medium_arena_byte_string);
  EXPECT_EQ(large_new_delete_byte_string, GetMediumStringView());
}

TEST_P(ByteStringTest, Swap) {
  using std::swap;
  ByteString empty_byte_string(GetAllocator());
  ByteString small_byte_string =
      ByteString(GetAllocator(), GetSmallStringView());
  ByteString medium_byte_string =
      ByteString(GetAllocator(), GetMediumStringView());
  ByteString large_byte_string =
      ByteString(GetAllocator(), GetMediumOrLargeCord());

  // Small <=> Small
  swap(empty_byte_string, small_byte_string);
  EXPECT_EQ(empty_byte_string, GetSmallStringView());
  EXPECT_EQ(small_byte_string, "");
  swap(empty_byte_string, small_byte_string);
  EXPECT_EQ(empty_byte_string, "");
  EXPECT_EQ(small_byte_string, GetSmallStringView());

  // Small <=> Medium
  swap(small_byte_string, medium_byte_string);
  EXPECT_EQ(small_byte_string, GetMediumStringView());
  EXPECT_EQ(medium_byte_string, GetSmallStringView());
  swap(small_byte_string, medium_byte_string);
  EXPECT_EQ(small_byte_string, GetSmallStringView());
  EXPECT_EQ(medium_byte_string, GetMediumStringView());

  // Small <=> Large
  swap(small_byte_string, large_byte_string);
  EXPECT_EQ(small_byte_string, GetMediumOrLargeCord());
  EXPECT_EQ(large_byte_string, GetSmallStringView());
  swap(small_byte_string, large_byte_string);
  EXPECT_EQ(small_byte_string, GetSmallStringView());
  EXPECT_EQ(large_byte_string, GetMediumOrLargeCord());

  // Medium <=> Medium
  static constexpr absl::string_view kDifferentMediumStringView =
      "A different string that is too large for the small string optimization!";
  ByteString other_medium_byte_string =
      ByteString(GetAllocator(), kDifferentMediumStringView);
  swap(medium_byte_string, other_medium_byte_string);
  EXPECT_EQ(medium_byte_string, kDifferentMediumStringView);
  EXPECT_EQ(other_medium_byte_string, GetMediumStringView());
  swap(medium_byte_string, other_medium_byte_string);
  EXPECT_EQ(medium_byte_string, GetMediumStringView());
  EXPECT_EQ(other_medium_byte_string, kDifferentMediumStringView);

  // Medium <=> Large
  swap(medium_byte_string, large_byte_string);
  EXPECT_EQ(medium_byte_string, GetMediumOrLargeCord());
  EXPECT_EQ(large_byte_string, GetMediumStringView());
  swap(medium_byte_string, large_byte_string);
  EXPECT_EQ(medium_byte_string, GetMediumStringView());
  EXPECT_EQ(large_byte_string, GetMediumOrLargeCord());

  // Large <=> Large
  const absl::Cord different_medium_or_large_cord =
      absl::Cord(kDifferentMediumStringView);
  ByteString other_large_byte_string =
      ByteString(GetAllocator(), different_medium_or_large_cord);
  swap(large_byte_string, other_large_byte_string);
  EXPECT_EQ(large_byte_string, different_medium_or_large_cord);
  EXPECT_EQ(other_large_byte_string, GetMediumStringView());
  swap(large_byte_string, other_large_byte_string);
  EXPECT_EQ(large_byte_string, GetMediumStringView());
  EXPECT_EQ(other_large_byte_string, different_medium_or_large_cord);

  // Miscellaneous cases not covered above. These do not swap a second time to
  // restore state, so they are destructive.
  // Small <=> Different Allocator Medium
  ByteString medium_new_delete_byte_string =
      ByteString(NewDeleteAllocator<>{}, kDifferentMediumStringView);
  swap(empty_byte_string, medium_new_delete_byte_string);
  EXPECT_EQ(empty_byte_string, kDifferentMediumStringView);
  EXPECT_EQ(medium_new_delete_byte_string, "");
  // Small <=> Different Allocator Large
  ByteString large_new_delete_byte_string =
      ByteString(NewDeleteAllocator<>{}, GetMediumOrLargeCord());
  swap(small_byte_string, large_new_delete_byte_string);
  EXPECT_EQ(small_byte_string, GetMediumOrLargeCord());
  EXPECT_EQ(large_new_delete_byte_string, GetSmallStringView());
  // Medium <=> Different Allocator Large
  large_new_delete_byte_string =
      ByteString(NewDeleteAllocator<>{}, different_medium_or_large_cord);
  swap(medium_byte_string, large_new_delete_byte_string);
  EXPECT_EQ(medium_byte_string, different_medium_or_large_cord);
  EXPECT_EQ(large_new_delete_byte_string, GetMediumStringView());
  // Medium <=> Different Allocator Medium
  medium_byte_string = ByteString(GetAllocator(), GetMediumStringView());
  medium_new_delete_byte_string =
      ByteString(NewDeleteAllocator<>{}, kDifferentMediumStringView);
  swap(medium_byte_string, medium_new_delete_byte_string);
  EXPECT_EQ(medium_byte_string, kDifferentMediumStringView);
  EXPECT_EQ(medium_new_delete_byte_string, GetMediumStringView());
}

TEST_P(ByteStringTest, FlattenSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.Flatten(), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, FlattenMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.Flatten(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
}

TEST_P(ByteStringTest, FlattenLarge) {
  if (GetAllocator().arena() != nullptr) {
    GTEST_SKIP();
  }
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_EQ(byte_string.Flatten(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
}

TEST_P(ByteStringTest, TryFlatSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_THAT(byte_string.TryFlat(), Optional(GetSmallStringView()));
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, TryFlatMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_THAT(byte_string.TryFlat(), Optional(GetMediumStringView()));
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
}

TEST_P(ByteStringTest, TryFlatLarge) {
  if (GetAllocator().arena() != nullptr) {
    GTEST_SKIP();
  }
  ByteString byte_string =
      ByteString(GetAllocator(), GetMediumOrLargeFragmentedCord());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_THAT(byte_string.TryFlat(), Eq(absl::nullopt));
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
}

TEST_P(ByteStringTest, Equals) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_TRUE(byte_string.Equals(GetMediumStringView()));
}

TEST_P(ByteStringTest, Compare) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.Compare(GetMediumStringView()), 0);
  EXPECT_EQ(byte_string.Compare(GetMediumOrLargeCord()), 0);
}

TEST_P(ByteStringTest, StartsWith) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_TRUE(byte_string.StartsWith(
      GetMediumStringView().substr(0, kSmallByteStringCapacity)));
  EXPECT_TRUE(byte_string.StartsWith(
      GetMediumOrLargeCord().Subcord(0, kSmallByteStringCapacity)));
}

TEST_P(ByteStringTest, EndsWith) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_TRUE(byte_string.EndsWith(
      GetMediumStringView().substr(kSmallByteStringCapacity)));
  EXPECT_TRUE(byte_string.EndsWith(GetMediumOrLargeCord().Subcord(
      kSmallByteStringCapacity,
      GetMediumOrLargeCord().size() - kSmallByteStringCapacity)));
}

TEST_P(ByteStringTest, Find) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());

  // Find string_view
  EXPECT_THAT(byte_string.Find("A string"), Optional(0));
  EXPECT_THAT(
      byte_string.Find("small string optimization!"),
      Optional(GetMediumStringView().find("small string optimization!")));
  EXPECT_THAT(byte_string.Find("not found"), Eq(absl::nullopt));
  EXPECT_THAT(byte_string.Find(""), Optional(0));
  EXPECT_THAT(byte_string.Find("", 3), Optional(3));
  EXPECT_THAT(byte_string.Find("A string", 1), Eq(absl::nullopt));

  // Find cord
  EXPECT_THAT(byte_string.Find(absl::Cord("A string")), Optional(0));
  EXPECT_THAT(
      byte_string.Find(absl::Cord("small string optimization!")),
      Optional(GetMediumStringView().find("small string optimization!")));
  EXPECT_THAT(
      byte_string.Find(absl::MakeFragmentedCord(
          {"A string", " that is too large for the small string optimization!",
           " extra"})),
      Eq(absl::nullopt));
  EXPECT_THAT(byte_string.Find(GetMediumOrLargeFragmentedCord()), Optional(0));
  EXPECT_THAT(byte_string.Find(absl::Cord("not found")), Eq(absl::nullopt));
  EXPECT_THAT(byte_string.Find(absl::Cord("")), Optional(0));
  EXPECT_THAT(byte_string.Find(absl::Cord(""), 3), Optional(3));
}

TEST_P(ByteStringTest, FindEdgeCases) {
  ByteString empty_byte_string(GetAllocator(), "");
  EXPECT_THAT(empty_byte_string.Find("a"), Eq(absl::nullopt));
  EXPECT_THAT(empty_byte_string.Find(""), Optional(0));
  ByteString cord_byte_string =
      ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_THAT(cord_byte_string.Find("not found"), Eq(absl::nullopt));
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());

  // Needle longer than haystack.
  EXPECT_THAT(byte_string.Find(std::string(byte_string.size() + 1, 'a')),
              Eq(absl::nullopt));

  // Needle at the end.
  absl::string_view suffix = "optimization!";
  EXPECT_THAT(byte_string.Find(suffix),
              Optional(byte_string.size() - suffix.size()));

  // pos at the end.
  EXPECT_THAT(byte_string.Find("a", byte_string.size()), Eq(absl::nullopt));
  EXPECT_THAT(byte_string.Find("", byte_string.size()),
              Optional(byte_string.size()));

  // Search in a cord-backed ByteString with pos > 0.
  EXPECT_THAT(cord_byte_string.Find("string", 1),
              Optional(GetMediumStringView().find("string", 1)));

  // Needle at the end of a cord-backed ByteString.
  absl::string_view suffix_sv = "optimization!";
  EXPECT_THAT(cord_byte_string.Find(suffix_sv),
              Optional(cord_byte_string.size() - suffix_sv.size()));
  EXPECT_THAT(cord_byte_string.Find(absl::Cord(suffix_sv)),
              Optional(cord_byte_string.size() - suffix_sv.size()));

  // Fragmented needle with empty first chunk.
  absl::Cord fragmented_with_empty_chunk;
  fragmented_with_empty_chunk.Append("");
  fragmented_with_empty_chunk.Append("A string");
  EXPECT_THAT(byte_string.Find(fragmented_with_empty_chunk), Optional(0));

  // Search with fragmented cord needle on string_view backed ByteString with
  // partial match.
  ByteString partial_match_haystack(GetAllocator(), "abababac");
  absl::Cord partial_match_needle = absl::MakeFragmentedCord({"aba", "c"});
  EXPECT_THAT(partial_match_haystack.Find(partial_match_needle), Optional(4));

  // Search with fragmented cord needle where first chunk is found but not
  // enough space for the rest.
  ByteString short_haystack(GetAllocator(), "abcdefg");
  absl::Cord needle_too_long = absl::MakeFragmentedCord({"ef", "gh"});
  EXPECT_THAT(short_haystack.Find(needle_too_long), Eq(absl::nullopt));

  // Search with a fragmented empty cord.
  absl::Cord fragmented_empty_cord = absl::MakeFragmentedCord({"", ""});
  EXPECT_THAT(byte_string.Find(fragmented_empty_cord), Optional(0));
  EXPECT_THAT(byte_string.Find(fragmented_empty_cord, 3), Optional(3));

  // Search for suffix in a fragmented cord.
  ByteString fragmented_cord_byte_string(GetAllocator(),
                                         GetMediumOrLargeFragmentedCord());
  EXPECT_THAT(fragmented_cord_byte_string.Find(suffix_sv),
              Optional(fragmented_cord_byte_string.size() - suffix_sv.size()));
  EXPECT_THAT(fragmented_cord_byte_string.Find(absl::Cord(suffix_sv)),
              Optional(fragmented_cord_byte_string.size() - suffix_sv.size()));
}

#ifndef NDEBUG
TEST_P(ByteStringTest, FindOutOfBounds) {
  ByteString byte_string = ByteString(GetAllocator(), "test");
  EXPECT_DEATH(byte_string.Find("t", 5), _);
}
#endif

TEST_P(ByteStringTest, Substring) {
  // small byte_string substring
  ByteString small_byte_string =
      ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(small_byte_string.Substring(1, 5),
            GetSmallStringView().substr(1, 4));
  EXPECT_EQ(small_byte_string.Substring(0, small_byte_string.size()),
            GetSmallStringView());
  EXPECT_EQ(small_byte_string.Substring(1, 1), "");
  // medium byte_string substring
  ByteString medium_byte_string =
      ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(medium_byte_string.Substring(2, 12),
            GetMediumStringView().substr(2, 10));
  EXPECT_EQ(medium_byte_string.Substring(0, medium_byte_string.size()),
            GetMediumStringView());
  // large byte_string substring
  ByteString large_byte_string =
      ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(large_byte_string.Substring(3, 15),
            GetMediumOrLargeCord().Subcord(3, 12));
  EXPECT_EQ(large_byte_string.Substring(0, large_byte_string.size()),
            GetMediumOrLargeCord());
  // substring with one parameter
  ByteString tacocat_byte_string = ByteString(GetAllocator(), "tacocat");
  EXPECT_EQ(tacocat_byte_string.Substring(4), "cat");
}

TEST_P(ByteStringTest, SubstringEdgeCases) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.Substring(byte_string.size(), byte_string.size()), "");
  EXPECT_EQ(byte_string.Substring(0, 0), "");
}

#ifndef NDEBUG
TEST_P(ByteStringTest, SubstringOutOfBounds) {
  ByteString byte_string = ByteString(GetAllocator(), "test");
  EXPECT_DEATH(static_cast<void>(byte_string.Substring(5, 5)), _);
  EXPECT_DEATH(static_cast<void>(byte_string.Substring(0, 5)), _);
  EXPECT_DEATH(static_cast<void>(byte_string.Substring(3, 2)), _);
}
#endif

TEST_P(ByteStringTest, RemovePrefixSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  byte_string.RemovePrefix(1);
  EXPECT_EQ(byte_string, GetSmallStringView().substr(1));
}

TEST_P(ByteStringTest, RemovePrefixMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  byte_string.RemovePrefix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(GetMediumStringView().size() -
                                         kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, RemovePrefixMediumOrLarge) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  byte_string.RemovePrefix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(GetMediumStringView().size() -
                                         kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, RemoveSuffixSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  byte_string.RemoveSuffix(1);
  EXPECT_EQ(byte_string,
            GetSmallStringView().substr(0, GetSmallStringView().size() - 1));
}

TEST_P(ByteStringTest, RemoveSuffixMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  byte_string.RemoveSuffix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(0, kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, RemoveSuffixMediumOrLarge) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  byte_string.RemoveSuffix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(0, kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, ToStringSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.ToString(), byte_string);
}

TEST_P(ByteStringTest, ToStringMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.ToString(), byte_string);
}

TEST_P(ByteStringTest, ToStringLarge) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.ToString(), byte_string);
}

TEST_P(ByteStringTest, ToStringViewSmall) {
  std::string scratch;
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.ToStringView(&scratch), GetSmallStringView());
}

TEST_P(ByteStringTest, ToStringViewMedium) {
  std::string scratch;
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.ToStringView(&scratch), GetMediumStringView());
}

TEST_P(ByteStringTest, ToStringViewLarge) {
  std::string scratch;
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.ToStringView(&scratch), GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, AsStringViewSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.AsStringView(), GetSmallStringView());
}

TEST_P(ByteStringTest, AsStringViewMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.AsStringView(), GetMediumStringView());
}

TEST_P(ByteStringTest, AsStringViewLarge) {
  ByteString byte_string = ByteString(GetMediumOrLargeCord());
  EXPECT_DEATH(byte_string.AsStringView(), _);
}

TEST_P(ByteStringTest, CopyToStringSmall) {
  std::string out;

  ByteString(GetAllocator(), GetSmallStringView()).CopyToString(&out);
  EXPECT_EQ(out, GetSmallStringView());
}

TEST_P(ByteStringTest, CopyToStringMedium) {
  std::string out;

  ByteString(GetAllocator(), GetMediumStringView()).CopyToString(&out);
  EXPECT_EQ(out, GetMediumStringView());
}

TEST_P(ByteStringTest, CopyToStringLarge) {
  std::string out;

  ByteString(GetAllocator(), GetMediumOrLargeCord()).CopyToString(&out);
  EXPECT_EQ(out, GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, AppendToStringSmall) {
  std::string out;

  ByteString(GetAllocator(), GetSmallStringView()).AppendToString(&out);
  EXPECT_EQ(out, GetSmallStringView());
}

TEST_P(ByteStringTest, AppendToStringMedium) {
  std::string out;

  ByteString(GetAllocator(), GetMediumStringView()).AppendToString(&out);
  EXPECT_EQ(out, GetMediumStringView());
}

TEST_P(ByteStringTest, AppendToStringLarge) {
  std::string out;

  ByteString(GetAllocator(), GetMediumOrLargeCord()).AppendToString(&out);
  EXPECT_EQ(out, GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, ToCordSmall) {
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.ToCord(), byte_string);
  EXPECT_EQ(std::move(byte_string).ToCord(), GetSmallStringView());
}

TEST_P(ByteStringTest, ToCordMedium) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.ToCord(), byte_string);
  EXPECT_EQ(std::move(byte_string).ToCord(), GetMediumStringView());
}

TEST_P(ByteStringTest, ToCordLarge) {
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.ToCord(), byte_string);
  EXPECT_EQ(std::move(byte_string).ToCord(), GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, CopyToCordSmall) {
  absl::Cord out;

  ByteString(GetAllocator(), GetSmallStringView()).CopyToCord(&out);
  EXPECT_EQ(out, GetSmallStringView());
}

TEST_P(ByteStringTest, CopyToCordMedium) {
  absl::Cord out;

  ByteString(GetAllocator(), GetMediumStringView()).CopyToCord(&out);
  EXPECT_EQ(out, GetMediumStringView());
}

TEST_P(ByteStringTest, CopyToCordLarge) {
  absl::Cord out;

  ByteString(GetAllocator(), GetMediumOrLargeCord()).CopyToCord(&out);
  EXPECT_EQ(out, GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, AppendToCordSmall) {
  absl::Cord out;

  ByteString(GetAllocator(), GetSmallStringView()).AppendToCord(&out);
  EXPECT_EQ(out, GetSmallStringView());
}

TEST_P(ByteStringTest, AppendToCordMedium) {
  absl::Cord out;

  ByteString(GetAllocator(), GetMediumStringView()).AppendToCord(&out);
  EXPECT_EQ(out, GetMediumStringView());
}

TEST_P(ByteStringTest, AppendToCordLarge) {
  absl::Cord out;

  ByteString(GetAllocator(), GetMediumOrLargeCord()).AppendToCord(&out);
  EXPECT_EQ(out, GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, CloneSmall) {
  google::protobuf::Arena arena;
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.Clone(&arena), byte_string);
}

TEST_P(ByteStringTest, CloneMedium) {
  google::protobuf::Arena arena;
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.Clone(&arena), byte_string);
}

TEST_P(ByteStringTest, CloneLarge) {
  google::protobuf::Arena arena;
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.Clone(&arena), byte_string);
}

TEST_P(ByteStringTest, LegacyByteStringSmall) {
  google::protobuf::Arena arena;
  ByteString byte_string = ByteString(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(LegacyByteString(byte_string, /*stable=*/false, &arena),
            GetSmallStringView());
  EXPECT_EQ(LegacyByteString(byte_string, /*stable=*/true, &arena),
            GetSmallStringView());
}

TEST_P(ByteStringTest, LegacyByteStringMedium) {
  google::protobuf::Arena arena;
  ByteString byte_string = ByteString(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(LegacyByteString(byte_string, /*stable=*/false, &arena),
            GetMediumStringView());
  EXPECT_EQ(LegacyByteString(byte_string, /*stable=*/true, &arena),
            GetMediumStringView());
}

TEST_P(ByteStringTest, LegacyByteStringLarge) {
  google::protobuf::Arena arena;
  ByteString byte_string = ByteString(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(LegacyByteString(byte_string, /*stable=*/false, &arena),
            GetMediumOrLargeCord());
  EXPECT_EQ(LegacyByteString(byte_string, /*stable=*/true, &arena),
            GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, HashValue) {
  EXPECT_EQ(absl::HashOf(ByteString(GetAllocator(), GetSmallStringView())),
            absl::HashOf(GetSmallStringView()));
  EXPECT_EQ(absl::HashOf(ByteString(GetAllocator(), GetMediumStringView())),
            absl::HashOf(GetMediumStringView()));
  EXPECT_EQ(absl::HashOf(ByteString(GetAllocator(), GetMediumOrLargeCord())),
            absl::HashOf(GetMediumOrLargeCord()));
}

INSTANTIATE_TEST_SUITE_P(ByteStringTest, ByteStringTest,
                         ::testing::Values(AllocatorKind::kNewDelete,
                                           AllocatorKind::kArena));

}  // namespace
}  // namespace cel::common_internal
