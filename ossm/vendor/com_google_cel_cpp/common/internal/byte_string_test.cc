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

struct ByteStringViewTestFriend {
  static ByteStringViewKind GetKind(ByteStringView byte_string_view) {
    return byte_string_view.GetKind();
  }
};

namespace {

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

TEST(ByteStringViewKind, Ostream) {
  {
    std::ostringstream out;
    out << ByteStringViewKind::kString;
    EXPECT_EQ(out.str(), "STRING");
  }
  {
    std::ostringstream out;
    out << ByteStringViewKind::kCord;
    EXPECT_EQ(out.str(), "CORD");
  }
}

class ByteStringTest : public TestWithParam<MemoryManagement>,
                       public ByteStringTestFriend {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case MemoryManagement::kPooling:
        return ArenaAllocator<>(&arena_);
      case MemoryManagement::kReferenceCounting:
        return NewDeleteAllocator<>{};
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
  ByteString byte_string = ByteString::Owned(GetAllocator(), "");
  EXPECT_THAT(byte_string, SizeIs(0));
  EXPECT_THAT(byte_string, IsEmpty());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, ConstructSmallCString) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallString().c_str());
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumCString) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumString().c_str());
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructSmallRValueString) {
  ByteString byte_string = ByteString::Owned(GetAllocator(), GetSmallString());
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructSmallLValueString) {
  ByteString byte_string = ByteString::Owned(
      GetAllocator(), static_cast<const std::string&>(GetSmallString()));
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumRValueString) {
  ByteString byte_string = ByteString::Owned(GetAllocator(), GetMediumString());
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumLValueString) {
  ByteString byte_string = ByteString::Owned(
      GetAllocator(), static_cast<const std::string&>(GetMediumString()));
  EXPECT_THAT(byte_string, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructSmallCord) {
  ByteString byte_string = ByteString::Owned(GetAllocator(), GetSmallCord());
  EXPECT_THAT(byte_string, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string, Not(IsEmpty()));
  EXPECT_EQ(byte_string, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringTest, ConstructMediumOrLargeCord) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
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
  ByteString byte_string =
      ByteString::Borrowed(Owner::None(), GetMediumStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumStringView());
#else
  EXPECT_DEBUG_DEATH(static_cast<void>(ByteString::Borrowed(
                         Owner::None(), GetMediumStringView())),
                     ::testing::_);
#endif
}

TEST(ByteStringTest, BorrowedUnownedCord) {
#ifdef NDEBUG
  ByteString byte_string =
      ByteString::Borrowed(Owner::None(), GetMediumOrLargeCord());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumOrLargeCord());
#else
  EXPECT_DEBUG_DEATH(static_cast<void>(ByteString::Borrowed(
                         Owner::None(), GetMediumOrLargeCord())),
                     ::testing::_);
#endif
}

TEST(ByteStringTest, BorrowedReferenceCountSmallString) {
  auto* refcount = new ReferenceCounted();
  Owner owner = Owner::ReferenceCount(refcount);
  StrongUnref(refcount);
  ByteString byte_string = ByteString::Borrowed(owner, GetSmallStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetSmallStringView());
}

TEST(ByteStringTest, BorrowedReferenceCountMediumString) {
  auto* refcount = new ReferenceCounted();
  Owner owner = Owner::ReferenceCount(refcount);
  StrongUnref(refcount);
  ByteString byte_string = ByteString::Borrowed(owner, GetMediumStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumStringView());
}

TEST(ByteStringTest, BorrowedArenaSmallString) {
  google::protobuf::Arena arena;
  ByteString byte_string =
      ByteString::Borrowed(Owner::Arena(&arena), GetSmallStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetArena(), &arena);
  EXPECT_EQ(byte_string, GetSmallStringView());
}

TEST(ByteStringTest, BorrowedArenaMediumString) {
  google::protobuf::Arena arena;
  ByteString byte_string =
      ByteString::Borrowed(Owner::Arena(&arena), GetMediumStringView());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), &arena);
  EXPECT_EQ(byte_string, GetMediumStringView());
}

TEST(ByteStringTest, BorrowedReferenceCountCord) {
  auto* refcount = new ReferenceCounted();
  Owner owner = Owner::ReferenceCount(refcount);
  StrongUnref(refcount);
  ByteString byte_string = ByteString::Borrowed(owner, GetMediumOrLargeCord());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_EQ(byte_string.GetArena(), nullptr);
  EXPECT_EQ(byte_string, GetMediumOrLargeCord());
}

TEST(ByteStringTest, BorrowedArenaCord) {
  google::protobuf::Arena arena;
  Owner owner = Owner::Arena(&arena);
  ByteString byte_string = ByteString::Borrowed(owner, GetMediumOrLargeCord());
  EXPECT_EQ(ByteStringTestFriend::GetKind(byte_string),
            ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetArena(), &arena);
  EXPECT_EQ(byte_string, GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, CopyFromByteStringView) {
  ByteString small_byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  ByteString medium_byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  ByteString large_byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());

  ByteString new_delete_byte_string(NewDeleteAllocator<>{});
  // Small <= Small
  new_delete_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(small_byte_string));
  // Small <= Medium
  new_delete_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Medium
  new_delete_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Large
  new_delete_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(large_byte_string));
  // Large <= Large
  new_delete_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(large_byte_string));
  // Large <= Small
  new_delete_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(small_byte_string));
  // Small <= Large
  new_delete_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(large_byte_string));
  // Large <= Medium
  new_delete_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Small
  new_delete_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(new_delete_byte_string, ByteStringView(small_byte_string));

  google::protobuf::Arena arena;
  ByteString arena_byte_string(ArenaAllocator<>{&arena});
  // Small <= Small
  arena_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(small_byte_string));
  // Small <= Medium
  arena_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Medium
  arena_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Large
  arena_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(large_byte_string));
  // Large <= Large
  arena_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(large_byte_string));
  // Large <= Small
  arena_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(small_byte_string));
  // Small <= Large
  arena_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(large_byte_string));
  // Large <= Medium
  arena_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Small
  arena_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(arena_byte_string, ByteStringView(small_byte_string));

  ByteString allocator_byte_string(GetAllocator());
  // Small <= Small
  allocator_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(small_byte_string));
  // Small <= Medium
  allocator_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Medium
  allocator_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Large
  allocator_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(large_byte_string));
  // Large <= Large
  allocator_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(large_byte_string));
  // Large <= Small
  allocator_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(small_byte_string));
  // Small <= Large
  allocator_byte_string = ByteStringView(large_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(large_byte_string));
  // Large <= Medium
  allocator_byte_string = ByteStringView(medium_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(medium_byte_string));
  // Medium <= Small
  allocator_byte_string = ByteStringView(small_byte_string);
  EXPECT_EQ(allocator_byte_string, ByteStringView(small_byte_string));

  // Miscellaneous cases not covered above.
  // Small <= Small Cord
  allocator_byte_string = ByteStringView(absl::Cord(GetSmallStringView()));
  EXPECT_EQ(allocator_byte_string, GetSmallStringView());
  allocator_byte_string = ByteStringView(medium_byte_string);
  // Medium <= Small Cord
  allocator_byte_string = ByteStringView(absl::Cord(GetSmallStringView()));
  EXPECT_EQ(allocator_byte_string, GetSmallStringView());
  // Large <= Small Cord
  allocator_byte_string = ByteStringView(large_byte_string);
  allocator_byte_string = ByteStringView(absl::Cord(GetSmallStringView()));
  EXPECT_EQ(allocator_byte_string, GetSmallStringView());
  // Large <= Medium Arena String
  ByteString large_new_delete_byte_string(NewDeleteAllocator<>{},
                                          GetMediumOrLargeCord());
  ByteString medium_arena_byte_string(ArenaAllocator<>{&arena},
                                      GetMediumStringView());
  large_new_delete_byte_string = ByteStringView(medium_arena_byte_string);
  EXPECT_EQ(large_new_delete_byte_string, medium_arena_byte_string);
}

TEST_P(ByteStringTest, CopyFromByteString) {
  ByteString small_byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  ByteString medium_byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  ByteString large_byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());

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
    return ByteString::Owned(GetAllocator(), GetSmallStringView());
  };
  const auto& medium_byte_string = [this]() {
    return ByteString::Owned(GetAllocator(), GetMediumStringView());
  };
  const auto& large_byte_string = [this]() {
    return ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
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
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  ByteString medium_byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  ByteString large_byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());

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
      ByteString::Owned(GetAllocator(), kDifferentMediumStringView);
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
      ByteString::Owned(GetAllocator(), different_medium_or_large_cord);
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
      ByteString::Owned(NewDeleteAllocator<>{}, kDifferentMediumStringView);
  swap(empty_byte_string, medium_new_delete_byte_string);
  EXPECT_EQ(empty_byte_string, kDifferentMediumStringView);
  EXPECT_EQ(medium_new_delete_byte_string, "");
  // Small <=> Different Allocator Large
  ByteString large_new_delete_byte_string =
      ByteString::Owned(NewDeleteAllocator<>{}, GetMediumOrLargeCord());
  swap(small_byte_string, large_new_delete_byte_string);
  EXPECT_EQ(small_byte_string, GetMediumOrLargeCord());
  EXPECT_EQ(large_new_delete_byte_string, GetSmallStringView());
  // Medium <=> Different Allocator Large
  large_new_delete_byte_string =
      ByteString::Owned(NewDeleteAllocator<>{}, different_medium_or_large_cord);
  swap(medium_byte_string, large_new_delete_byte_string);
  EXPECT_EQ(medium_byte_string, different_medium_or_large_cord);
  EXPECT_EQ(large_new_delete_byte_string, GetMediumStringView());
  // Medium <=> Different Allocator Medium
  medium_byte_string = ByteString::Owned(GetAllocator(), GetMediumStringView());
  medium_new_delete_byte_string =
      ByteString::Owned(NewDeleteAllocator<>{}, kDifferentMediumStringView);
  swap(medium_byte_string, medium_new_delete_byte_string);
  EXPECT_EQ(medium_byte_string, kDifferentMediumStringView);
  EXPECT_EQ(medium_new_delete_byte_string, GetMediumStringView());
}

TEST_P(ByteStringTest, FlattenSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.Flatten(), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, FlattenMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.Flatten(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
}

TEST_P(ByteStringTest, FlattenLarge) {
  if (GetAllocator().arena() != nullptr) {
    GTEST_SKIP();
  }
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_EQ(byte_string.Flatten(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
}

TEST_P(ByteStringTest, TryFlatSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_THAT(byte_string.TryFlat(), Optional(GetSmallStringView()));
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, TryFlatMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_THAT(byte_string.TryFlat(), Optional(GetMediumStringView()));
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
}

TEST_P(ByteStringTest, TryFlatLarge) {
  if (GetAllocator().arena() != nullptr) {
    GTEST_SKIP();
  }
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeFragmentedCord());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
  EXPECT_THAT(byte_string.TryFlat(), Eq(absl::nullopt));
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kLarge);
}

TEST_P(ByteStringTest, GetFlatSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  std::string scratch;
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string.GetFlat(scratch), GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
}

TEST_P(ByteStringTest, GetFlatMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  std::string scratch;
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  EXPECT_EQ(byte_string.GetFlat(scratch), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
}

TEST_P(ByteStringTest, GetFlatLarge) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  std::string scratch;
  EXPECT_EQ(byte_string.GetFlat(scratch), GetMediumStringView());
}

TEST_P(ByteStringTest, GetFlatLargeFragmented) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeFragmentedCord());
  std::string scratch;
  EXPECT_EQ(byte_string.GetFlat(scratch), GetMediumStringView());
}

TEST_P(ByteStringTest, Equals) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_TRUE(byte_string.Equals(GetMediumStringView()));
}

TEST_P(ByteStringTest, Compare) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.Compare(GetMediumStringView()), 0);
  EXPECT_EQ(byte_string.Compare(GetMediumOrLargeCord()), 0);
}

TEST_P(ByteStringTest, StartsWith) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_TRUE(byte_string.StartsWith(
      GetMediumStringView().substr(0, kSmallByteStringCapacity)));
  EXPECT_TRUE(byte_string.StartsWith(
      GetMediumOrLargeCord().Subcord(0, kSmallByteStringCapacity)));
}

TEST_P(ByteStringTest, EndsWith) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_TRUE(byte_string.EndsWith(
      GetMediumStringView().substr(kSmallByteStringCapacity)));
  EXPECT_TRUE(byte_string.EndsWith(GetMediumOrLargeCord().Subcord(
      kSmallByteStringCapacity,
      GetMediumOrLargeCord().size() - kSmallByteStringCapacity)));
}

TEST_P(ByteStringTest, RemovePrefixSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  byte_string.RemovePrefix(1);
  EXPECT_EQ(byte_string, GetSmallStringView().substr(1));
}

TEST_P(ByteStringTest, RemovePrefixMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  byte_string.RemovePrefix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(GetMediumStringView().size() -
                                         kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, RemovePrefixMediumOrLarge) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  byte_string.RemovePrefix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(GetMediumStringView().size() -
                                         kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, RemoveSuffixSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  byte_string.RemoveSuffix(1);
  EXPECT_EQ(byte_string,
            GetSmallStringView().substr(0, GetSmallStringView().size() - 1));
}

TEST_P(ByteStringTest, RemoveSuffixMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kMedium);
  byte_string.RemoveSuffix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(0, kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, RemoveSuffixMediumOrLarge) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  byte_string.RemoveSuffix(byte_string.size() - kSmallByteStringCapacity);
  EXPECT_EQ(GetKind(byte_string), ByteStringKind::kSmall);
  EXPECT_EQ(byte_string,
            GetMediumStringView().substr(0, kSmallByteStringCapacity));
}

TEST_P(ByteStringTest, ToStringSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.ToString(), byte_string);
}

TEST_P(ByteStringTest, ToStringMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.ToString(), byte_string);
}

TEST_P(ByteStringTest, ToStringLarge) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.ToString(), byte_string);
}

TEST_P(ByteStringTest, ToCordSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  EXPECT_EQ(byte_string.ToCord(), byte_string);
  EXPECT_EQ(std::move(byte_string).ToCord(), GetSmallStringView());
}

TEST_P(ByteStringTest, ToCordMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  EXPECT_EQ(byte_string.ToCord(), byte_string);
  EXPECT_EQ(std::move(byte_string).ToCord(), GetMediumStringView());
}

TEST_P(ByteStringTest, ToCordLarge) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  EXPECT_EQ(byte_string.ToCord(), byte_string);
  EXPECT_EQ(std::move(byte_string).ToCord(), GetMediumOrLargeCord());
}

TEST_P(ByteStringTest, HashValue) {
  EXPECT_EQ(
      absl::HashOf(ByteString::Owned(GetAllocator(), GetSmallStringView())),
      absl::HashOf(GetSmallStringView()));
  EXPECT_EQ(
      absl::HashOf(ByteString::Owned(GetAllocator(), GetMediumStringView())),
      absl::HashOf(GetMediumStringView()));
  EXPECT_EQ(
      absl::HashOf(ByteString::Owned(GetAllocator(), GetMediumOrLargeCord())),
      absl::HashOf(GetMediumOrLargeCord()));
}

INSTANTIATE_TEST_SUITE_P(
    ByteStringTest, ByteStringTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting));

class ByteStringViewTest : public TestWithParam<MemoryManagement>,
                           public ByteStringViewTestFriend {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case MemoryManagement::kPooling:
        return ArenaAllocator<>(&arena_);
      case MemoryManagement::kReferenceCounting:
        return NewDeleteAllocator<>{};
    }
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_P(ByteStringViewTest, Default) {
  ByteStringView byte_String_view;
  EXPECT_THAT(byte_String_view, SizeIs(0));
  EXPECT_THAT(byte_String_view, IsEmpty());
  EXPECT_EQ(GetKind(byte_String_view), ByteStringViewKind::kString);
}

TEST_P(ByteStringViewTest, String) {
  ByteStringView byte_string_view(GetSmallStringView());
  EXPECT_THAT(byte_string_view, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string_view, Not(IsEmpty()));
  EXPECT_EQ(byte_string_view, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string_view), ByteStringViewKind::kString);
  EXPECT_EQ(byte_string_view.GetArena(), nullptr);
}

TEST_P(ByteStringViewTest, Cord) {
  ByteStringView byte_string_view(GetMediumOrLargeCord());
  EXPECT_THAT(byte_string_view, SizeIs(GetMediumOrLargeCord().size()));
  EXPECT_THAT(byte_string_view, Not(IsEmpty()));
  EXPECT_EQ(byte_string_view, GetMediumOrLargeCord());
  EXPECT_EQ(GetKind(byte_string_view), ByteStringViewKind::kCord);
  EXPECT_EQ(byte_string_view.GetArena(), nullptr);
}

TEST_P(ByteStringViewTest, ByteStringSmall) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  ByteStringView byte_string_view(byte_string);
  EXPECT_THAT(byte_string_view, SizeIs(GetSmallStringView().size()));
  EXPECT_THAT(byte_string_view, Not(IsEmpty()));
  EXPECT_EQ(byte_string_view, GetSmallStringView());
  EXPECT_EQ(GetKind(byte_string_view), ByteStringViewKind::kString);
  EXPECT_EQ(byte_string_view.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringViewTest, ByteStringMedium) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumStringView());
  ByteStringView byte_string_view(byte_string);
  EXPECT_THAT(byte_string_view, SizeIs(GetMediumStringView().size()));
  EXPECT_THAT(byte_string_view, Not(IsEmpty()));
  EXPECT_EQ(byte_string_view, GetMediumStringView());
  EXPECT_EQ(GetKind(byte_string_view), ByteStringViewKind::kString);
  EXPECT_EQ(byte_string_view.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringViewTest, ByteStringLarge) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  ByteStringView byte_string_view(byte_string);
  EXPECT_THAT(byte_string_view, SizeIs(GetMediumOrLargeCord().size()));
  EXPECT_THAT(byte_string_view, Not(IsEmpty()));
  EXPECT_EQ(byte_string_view, GetMediumOrLargeCord());
  EXPECT_EQ(byte_string_view.ToCord(), byte_string_view);
  if (GetAllocator().arena() == nullptr) {
    EXPECT_EQ(GetKind(byte_string_view), ByteStringViewKind::kCord);
  } else {
    EXPECT_EQ(GetKind(byte_string_view), ByteStringViewKind::kString);
  }
  EXPECT_EQ(byte_string_view.GetArena(), GetAllocator().arena());
}

TEST_P(ByteStringViewTest, TryFlatString) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  ByteStringView byte_string_view(byte_string);
  EXPECT_THAT(byte_string_view.TryFlat(), Optional(GetSmallStringView()));
}

TEST_P(ByteStringViewTest, TryFlatCord) {
  if (GetAllocator().arena() != nullptr) {
    GTEST_SKIP();
  }
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeFragmentedCord());
  ByteStringView byte_string_view(byte_string);
  EXPECT_THAT(byte_string_view.TryFlat(), Eq(absl::nullopt));
}

TEST_P(ByteStringViewTest, GetFlatString) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetSmallStringView());
  ByteStringView byte_string_view(byte_string);
  std::string scratch;
  EXPECT_EQ(byte_string_view.GetFlat(scratch), GetSmallStringView());
}

TEST_P(ByteStringViewTest, GetFlatCord) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeCord());
  ByteStringView byte_string_view(byte_string);
  std::string scratch;
  EXPECT_EQ(byte_string_view.GetFlat(scratch), GetMediumStringView());
}

TEST_P(ByteStringViewTest, GetFlatLargeFragmented) {
  ByteString byte_string =
      ByteString::Owned(GetAllocator(), GetMediumOrLargeFragmentedCord());
  ByteStringView byte_string_view(byte_string);
  std::string scratch;
  EXPECT_EQ(byte_string_view.GetFlat(scratch), GetMediumStringView());
}

TEST_P(ByteStringViewTest, RemovePrefixString) {
  ByteStringView byte_string_view(GetSmallStringView());
  byte_string_view.RemovePrefix(1);
  EXPECT_EQ(byte_string_view, GetSmallStringView().substr(1));
}

TEST_P(ByteStringViewTest, RemovePrefixCord) {
  ByteStringView byte_string_view(GetMediumOrLargeCord());
  byte_string_view.RemovePrefix(1);
  EXPECT_EQ(byte_string_view, GetMediumOrLargeCord().Subcord(
                                  1, GetMediumOrLargeCord().size() - 1));
}

TEST_P(ByteStringViewTest, RemoveSuffixString) {
  ByteStringView byte_string_view(GetSmallStringView());
  byte_string_view.RemoveSuffix(1);
  EXPECT_EQ(byte_string_view,
            GetSmallStringView().substr(0, GetSmallStringView().size() - 1));
}

TEST_P(ByteStringViewTest, RemoveSuffixCord) {
  ByteStringView byte_string_view(GetMediumOrLargeCord());
  byte_string_view.RemoveSuffix(1);
  EXPECT_EQ(byte_string_view, GetMediumOrLargeCord().Subcord(
                                  0, GetMediumOrLargeCord().size() - 1));
}

TEST_P(ByteStringViewTest, ToStringString) {
  ByteStringView byte_string_view(GetSmallStringView());
  EXPECT_EQ(byte_string_view.ToString(), byte_string_view);
}

TEST_P(ByteStringViewTest, ToStringCord) {
  ByteStringView byte_string_view(GetMediumOrLargeCord());
  EXPECT_EQ(byte_string_view.ToString(), byte_string_view);
}

TEST_P(ByteStringViewTest, ToCordString) {
  ByteString byte_string(GetAllocator(), GetMediumStringView());
  ByteStringView byte_string_view(byte_string);
  EXPECT_EQ(byte_string_view.ToCord(), byte_string_view);
}

TEST_P(ByteStringViewTest, ToCordCord) {
  ByteStringView byte_string_view(GetMediumOrLargeCord());
  EXPECT_EQ(byte_string_view.ToCord(), byte_string_view);
}

TEST_P(ByteStringViewTest, HashValue) {
  EXPECT_EQ(absl::HashOf(ByteStringView(GetSmallStringView())),
            absl::HashOf(GetSmallStringView()));
  EXPECT_EQ(absl::HashOf(ByteStringView(GetMediumStringView())),
            absl::HashOf(GetMediumStringView()));
  EXPECT_EQ(absl::HashOf(ByteStringView(GetMediumOrLargeCord())),
            absl::HashOf(GetMediumOrLargeCord()));
}

INSTANTIATE_TEST_SUITE_P(
    ByteStringViewTest, ByteStringViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting));

}  // namespace
}  // namespace cel::common_internal
