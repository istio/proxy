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

#include "common/allocator.h"

#include <type_traits>

#include "absl/strings/str_cat.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::testing::NotNull;

TEST(AllocatorKind, AbslStringify) {
  EXPECT_EQ(absl::StrCat(AllocatorKind::kArena), "ARENA");
  EXPECT_EQ(absl::StrCat(AllocatorKind::kNewDelete), "NEW_DELETE");
  EXPECT_EQ(absl::StrCat(static_cast<AllocatorKind>(0)), "ERROR");
}

TEST(NewDeleteAllocator, Bytes) {
  auto allocator = NewDeleteAllocator<>();
  void* p = allocator.allocate_bytes(17, 8);
  EXPECT_THAT(p, NotNull());
  allocator.deallocate_bytes(p, 17, 8);
}

TEST(ArenaAllocator, Bytes) {
  google::protobuf::Arena arena;
  auto allocator = ArenaAllocator<>(&arena);
  void* p = allocator.allocate_bytes(17, 8);
  EXPECT_THAT(p, NotNull());
  allocator.deallocate_bytes(p, 17, 8);
}

struct TrivialObject {
  char data[17];
};

TEST(NewDeleteAllocator, NewDeleteObject) {
  auto allocator = NewDeleteAllocator<>();
  auto* p = allocator.new_object<TrivialObject>();
  EXPECT_THAT(p, NotNull());
  allocator.delete_object(p);
}

TEST(ArenaAllocator, NewDeleteObject) {
  google::protobuf::Arena arena;
  auto allocator = ArenaAllocator<>(&arena);
  auto* p = allocator.new_object<TrivialObject>();
  EXPECT_THAT(p, NotNull());
  allocator.delete_object(p);
}

TEST(NewDeleteAllocator, Object) {
  auto allocator = NewDeleteAllocator<>();
  auto* p = allocator.allocate_object<TrivialObject>();
  EXPECT_THAT(p, NotNull());
  allocator.deallocate_object(p);
}

TEST(ArenaAllocator, Object) {
  google::protobuf::Arena arena;
  auto allocator = ArenaAllocator<>(&arena);
  auto* p = allocator.allocate_object<TrivialObject>();
  EXPECT_THAT(p, NotNull());
  allocator.deallocate_object(p);
}

TEST(NewDeleteAllocator, ObjectArray) {
  auto allocator = NewDeleteAllocator<>();
  auto* p = allocator.allocate_object<TrivialObject>(2);
  EXPECT_THAT(p, NotNull());
  allocator.deallocate_object(p, 2);
}

TEST(ArenaAllocator, ObjectArray) {
  google::protobuf::Arena arena;
  auto allocator = ArenaAllocator<>(&arena);
  auto* p = allocator.allocate_object<TrivialObject>(2);
  EXPECT_THAT(p, NotNull());
  allocator.deallocate_object(p, 2);
}

TEST(NewDeleteAllocator, T) {
  auto allocator = NewDeleteAllocatorFor<TrivialObject>();
  auto* p = allocator.allocate(1);
  EXPECT_THAT(p, NotNull());
  allocator.construct(p);
  allocator.destroy(p);
  allocator.deallocate(p, 1);
}

TEST(ArenaAllocator, T) {
  google::protobuf::Arena arena;
  auto allocator = ArenaAllocatorFor<TrivialObject>(&arena);
  auto* p = allocator.allocate(1);
  EXPECT_THAT(p, NotNull());
  allocator.construct(p);
  allocator.destroy(p);
  allocator.deallocate(p, 1);
}

TEST(NewDeleteAllocator, CopyConstructible) {
  EXPECT_TRUE(
      (std::is_trivially_constructible_v<NewDeleteAllocator<void>,
                                         const NewDeleteAllocator<void>&>));
  EXPECT_TRUE(
      (std::is_trivially_constructible_v<NewDeleteAllocator<bool>,
                                         const NewDeleteAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<NewDeleteAllocator<void>,
                                       const NewDeleteAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<NewDeleteAllocator<bool>,
                                       const NewDeleteAllocator<void>&>));
  EXPECT_TRUE((std::is_constructible_v<NewDeleteAllocator<char>,
                                       const NewDeleteAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<NewDeleteAllocator<bool>,
                                       const NewDeleteAllocator<char>&>));
}

TEST(ArenaAllocator, CopyConstructible) {
  EXPECT_TRUE((std::is_trivially_constructible_v<ArenaAllocator<void>,
                                                 const ArenaAllocator<void>&>));
  EXPECT_TRUE((std::is_trivially_constructible_v<ArenaAllocator<bool>,
                                                 const ArenaAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<ArenaAllocator<void>,
                                       const ArenaAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<ArenaAllocator<bool>,
                                       const ArenaAllocator<void>&>));
  EXPECT_TRUE((std::is_constructible_v<ArenaAllocator<char>,
                                       const ArenaAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<ArenaAllocator<bool>,
                                       const ArenaAllocator<char>&>));
}

TEST(Allocator, CopyConstructible) {
  EXPECT_TRUE((std::is_trivially_constructible_v<Allocator<void>,
                                                 const Allocator<void>&>));
  EXPECT_TRUE((std::is_trivially_constructible_v<Allocator<bool>,
                                                 const Allocator<bool>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<void>, const Allocator<bool>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<bool>, const Allocator<void>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<char>, const Allocator<bool>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<bool>, const Allocator<char>&>));

  EXPECT_TRUE((std::is_constructible_v<Allocator<void>,
                                       const NewDeleteAllocator<void>&>));
  EXPECT_TRUE((std::is_constructible_v<Allocator<void>,
                                       const NewDeleteAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<Allocator<bool>,
                                       const NewDeleteAllocator<void>&>));
  EXPECT_TRUE((std::is_constructible_v<Allocator<bool>,
                                       const NewDeleteAllocator<bool>&>));
  EXPECT_TRUE((std::is_constructible_v<Allocator<bool>,
                                       const NewDeleteAllocator<char>&>));
  EXPECT_TRUE((std::is_constructible_v<Allocator<char>,
                                       const NewDeleteAllocator<bool>&>));

  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<void>, const ArenaAllocator<void>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<void>, const ArenaAllocator<bool>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<bool>, const ArenaAllocator<void>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<bool>, const ArenaAllocator<bool>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<bool>, const ArenaAllocator<char>&>));
  EXPECT_TRUE(
      (std::is_constructible_v<Allocator<char>, const ArenaAllocator<bool>&>));
}

}  // namespace
}  // namespace cel
