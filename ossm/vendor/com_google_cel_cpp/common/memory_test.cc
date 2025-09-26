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

// This header contains primitives for reference counting, roughly equivalent to
// the primitives used to implement `std::shared_ptr`. These primitives should
// not be used directly in most cases, instead `cel::ManagedMemory` should be
// used instead.

#include "common/memory.h"

#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "common/allocator.h"
#include "common/data.h"
#include "common/internal/reference_count.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

#ifdef ABSL_HAVE_EXCEPTIONS
#include <stdexcept>
#endif

namespace cel {
namespace {

using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;

TEST(Owner, None) {
  EXPECT_THAT(Owner::None(), IsFalse());
  EXPECT_THAT(Owner::None().arena(), IsNull());
}

TEST(Owner, Allocator) {
  google::protobuf::Arena arena;
  EXPECT_THAT(Owner::Allocator(NewDeleteAllocator<>{}), IsFalse());
  EXPECT_THAT(Owner::Allocator(ArenaAllocator<>{&arena}), IsTrue());
}

TEST(Owner, Arena) {
  google::protobuf::Arena arena;
  EXPECT_THAT(Owner::Arena(&arena), IsTrue());
  EXPECT_EQ(Owner::Arena(&arena).arena(), &arena);
}

TEST(Owner, ReferenceCount) {
  auto* refcount = new common_internal::ReferenceCounted();
  EXPECT_THAT(Owner::ReferenceCount(refcount), IsTrue());
  EXPECT_THAT(Owner::ReferenceCount(refcount).arena(), IsNull());
  common_internal::StrongUnref(refcount);
}

TEST(Owner, Equality) {
  google::protobuf::Arena arena1;
  google::protobuf::Arena arena2;
  EXPECT_EQ(Owner::None(), Owner::None());
  EXPECT_EQ(Owner::Allocator(NewDeleteAllocator<>{}), Owner::None());
  EXPECT_EQ(Owner::Arena(&arena1), Owner::Arena(&arena1));
  EXPECT_NE(Owner::Arena(&arena1), Owner::None());
  EXPECT_NE(Owner::None(), Owner::Arena(&arena1));
  EXPECT_NE(Owner::Arena(&arena1), Owner::Arena(&arena2));
  EXPECT_EQ(Owner::Allocator(ArenaAllocator<>{&arena1}), Owner::Arena(&arena1));
}

TEST(Borrower, None) {
  EXPECT_THAT(Borrower::None(), IsFalse());
  EXPECT_THAT(Borrower::None().arena(), IsNull());
}

TEST(Borrower, Allocator) {
  google::protobuf::Arena arena;
  EXPECT_THAT(Borrower::Allocator(NewDeleteAllocator<>{}), IsFalse());
  EXPECT_THAT(Borrower::Allocator(ArenaAllocator<>{&arena}), IsTrue());
}

TEST(Borrower, Arena) {
  google::protobuf::Arena arena;
  EXPECT_THAT(Borrower::Arena(&arena), IsTrue());
  EXPECT_EQ(Borrower::Arena(&arena).arena(), &arena);
}

TEST(Borrower, ReferenceCount) {
  auto* refcount = new common_internal::ReferenceCounted();
  EXPECT_THAT(Borrower::ReferenceCount(refcount), IsTrue());
  EXPECT_THAT(Borrower::ReferenceCount(refcount).arena(), IsNull());
  common_internal::StrongUnref(refcount);
}

TEST(Borrower, Equality) {
  google::protobuf::Arena arena1;
  google::protobuf::Arena arena2;
  EXPECT_EQ(Borrower::None(), Borrower::None());
  EXPECT_EQ(Borrower::Allocator(NewDeleteAllocator<>{}), Borrower::None());
  EXPECT_EQ(Borrower::Arena(&arena1), Borrower::Arena(&arena1));
  EXPECT_NE(Borrower::Arena(&arena1), Borrower::None());
  EXPECT_NE(Borrower::None(), Borrower::Arena(&arena1));
  EXPECT_NE(Borrower::Arena(&arena1), Borrower::Arena(&arena2));
  EXPECT_EQ(Borrower::Allocator(ArenaAllocator<>{&arena1}),
            Borrower::Arena(&arena1));
}

TEST(OwnerBorrower, CopyConstruct) {
  auto* refcount = new common_internal::ReferenceCounted();
  Owner owner1 = Owner::ReferenceCount(refcount);
  common_internal::StrongUnref(refcount);
  Owner owner2(owner1);
  Borrower borrower(owner1);
  EXPECT_EQ(owner1, owner2);
  EXPECT_EQ(owner1, borrower);
  EXPECT_EQ(borrower, owner1);
}

TEST(OwnerBorrower, MoveConstruct) {
  auto* refcount = new common_internal::ReferenceCounted();
  Owner owner1 = Owner::ReferenceCount(refcount);
  common_internal::StrongUnref(refcount);
  Owner owner2(std::move(owner1));
  Borrower borrower(owner2);
  EXPECT_EQ(owner2, borrower);
  EXPECT_EQ(borrower, owner2);
}

TEST(OwnerBorrower, CopyAssign) {
  auto* refcount = new common_internal::ReferenceCounted();
  Owner owner1 = Owner::ReferenceCount(refcount);
  common_internal::StrongUnref(refcount);
  Owner owner2;
  owner2 = owner1;
  Borrower borrower(owner1);
  EXPECT_EQ(owner1, owner2);
  EXPECT_EQ(owner1, borrower);
  EXPECT_EQ(borrower, owner1);
}

TEST(OwnerBorrower, MoveAssign) {
  auto* refcount = new common_internal::ReferenceCounted();
  Owner owner1 = Owner::ReferenceCount(refcount);
  common_internal::StrongUnref(refcount);
  Owner owner2;
  owner2 = std::move(owner1);
  Borrower borrower(owner2);
  EXPECT_EQ(owner2, borrower);
  EXPECT_EQ(borrower, owner2);
}

TEST(Unique, ToAddress) {
  Unique<bool> unique;
  EXPECT_EQ(cel::to_address(unique), nullptr);
  unique = AllocateUnique<bool>(NewDeleteAllocator<>{});
  EXPECT_EQ(cel::to_address(unique), unique.operator->());
}

class OwnedTest : public TestWithParam<AllocatorKind> {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        return ArenaAllocator<>{&arena_};
      case AllocatorKind::kNewDelete:
        return NewDeleteAllocator<>{};
    }
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_P(OwnedTest, Default) {
  Owned<Data> owned;
  EXPECT_FALSE(owned);
  EXPECT_EQ(cel::to_address(owned), nullptr);
  EXPECT_FALSE(owned != nullptr);
  EXPECT_FALSE(nullptr != owned);
}

class TestData final : public Data {
 public:
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;

  TestData() noexcept : Data() {}

  explicit TestData(google::protobuf::Arena* absl_nullable arena) noexcept
      : Data(arena) {}
};

TEST_P(OwnedTest, AllocateSharedData) {
  auto owned = AllocateShared<TestData>(GetAllocator());
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_EQ(Owner(owned).arena(), GetAllocator().arena());
  EXPECT_EQ(Borrower(owned).arena(), GetAllocator().arena());
}

TEST_P(OwnedTest, AllocateSharedMessageLite) {
  auto owned = AllocateShared<google::protobuf::Value>(GetAllocator());
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_EQ(Owner(owned).arena(), GetAllocator().arena());
  EXPECT_EQ(Borrower(owned).arena(), GetAllocator().arena());
}

TEST_P(OwnedTest, WrapSharedData) {
  auto owned =
      WrapShared(google::protobuf::Arena::Create<TestData>(GetAllocator().arena()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_EQ(Owner(owned).arena(), GetAllocator().arena());
  EXPECT_EQ(Borrower(owned).arena(), GetAllocator().arena());
}

TEST_P(OwnedTest, WrapSharedMessageLite) {
  auto owned = WrapShared(
      google::protobuf::Arena::Create<google::protobuf::Value>(GetAllocator().arena()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_EQ(Owner(owned).arena(), GetAllocator().arena());
  EXPECT_EQ(Borrower(owned).arena(), GetAllocator().arena());
}

TEST_P(OwnedTest, SharedFromUniqueData) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_EQ(Owner(owned).arena(), GetAllocator().arena());
  EXPECT_EQ(Borrower(owned).arena(), GetAllocator().arena());
}

TEST_P(OwnedTest, SharedFromUniqueMessageLite) {
  auto owned = Owned(AllocateUnique<google::protobuf::Value>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_EQ(Owner(owned).arena(), GetAllocator().arena());
  EXPECT_EQ(Borrower(owned).arena(), GetAllocator().arena());
}

TEST_P(OwnedTest, CopyConstruct) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> copied_owned(owned);
  EXPECT_EQ(copied_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, MoveConstruct) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> moved_owned(std::move(owned));
  EXPECT_EQ(moved_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, CopyConstructOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<Data> copied_owned(owned);
  EXPECT_EQ(copied_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, MoveConstructOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<Data> moved_owned(std::move(owned));
  EXPECT_EQ(moved_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, ConstructBorrowed) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> borrowed_owned(Borrowed<TestData>{owned});
  EXPECT_EQ(borrowed_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, ConstructOwner) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> owner_owned(Owner(owned), cel::to_address(owned));
  EXPECT_EQ(owner_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, ConstructNullPtr) {
  Owned<Data> owned(nullptr);
  EXPECT_EQ(owned, nullptr);
}

TEST_P(OwnedTest, CopyAssign) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> copied_owned;
  copied_owned = owned;
  EXPECT_EQ(copied_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, MoveAssign) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> moved_owned;
  moved_owned = std::move(owned);
  EXPECT_EQ(moved_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, CopyAssignOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<Data> copied_owned;
  copied_owned = owned;
  EXPECT_EQ(copied_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, MoveAssignOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<Data> moved_owned;
  moved_owned = std::move(owned);
  EXPECT_EQ(moved_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, AssignBorrowed) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Owned<TestData> borrowed_owned;
  borrowed_owned = Borrowed<TestData>{owned};
  EXPECT_EQ(borrowed_owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, AssignUnique) {
  Owned<TestData> owned;
  owned = AllocateUnique<TestData>(GetAllocator());
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
}

TEST_P(OwnedTest, AssignNullPtr) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  EXPECT_TRUE(owned);
  owned = nullptr;
  EXPECT_FALSE(owned);
}

INSTANTIATE_TEST_SUITE_P(OwnedTest, OwnedTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete));

class BorrowedTest : public TestWithParam<AllocatorKind> {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        return ArenaAllocator<>{&arena_};
      case AllocatorKind::kNewDelete:
        return NewDeleteAllocator<>{};
    }
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_P(BorrowedTest, Default) {
  Borrowed<Data> borrowed;
  EXPECT_FALSE(borrowed);
  EXPECT_EQ(cel::to_address(borrowed), nullptr);
  EXPECT_FALSE(borrowed != nullptr);
  EXPECT_FALSE(nullptr != borrowed);
}

TEST_P(BorrowedTest, CopyConstruct) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<TestData> copied_borrowed(borrowed);
  EXPECT_EQ(copied_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, MoveConstruct) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<TestData> moved_borrowed(std::move(borrowed));
  EXPECT_EQ(moved_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, CopyConstructOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<Data> copied_borrowed(borrowed);
  EXPECT_EQ(copied_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, MoveConstructOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<Data> moved_borrowed(std::move(borrowed));
  EXPECT_EQ(moved_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, ConstructNullPtr) {
  Borrowed<TestData> borrowed(nullptr);
  EXPECT_FALSE(borrowed);
}

TEST_P(BorrowedTest, CopyAssign) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<TestData> copied_borrowed;
  copied_borrowed = borrowed;
  EXPECT_EQ(copied_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, MoveAssign) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<TestData> moved_borrowed;
  moved_borrowed = std::move(borrowed);
  EXPECT_EQ(moved_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, CopyAssignOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<Data> copied_borrowed;
  copied_borrowed = borrowed;
  EXPECT_EQ(copied_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, MoveAssignOther) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  auto borrowed = Borrowed(owned);
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
  Borrowed<Data> moved_borrowed;
  moved_borrowed = std::move(borrowed);
  EXPECT_EQ(moved_borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, AssignOwned) {
  auto owned = Owned(AllocateUnique<TestData>(GetAllocator()));
  EXPECT_EQ(owned->GetArena(), GetAllocator().arena());
  Borrowed<Data> borrowed = owned;
  EXPECT_EQ(borrowed->GetArena(), GetAllocator().arena());
}

TEST_P(BorrowedTest, AssignNullPtr) {
  Borrowed<TestData> borrowed;
  borrowed = nullptr;
  EXPECT_FALSE(borrowed);
}

INSTANTIATE_TEST_SUITE_P(BorrowedTest, BorrowedTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete));

}  // namespace
}  // namespace cel
