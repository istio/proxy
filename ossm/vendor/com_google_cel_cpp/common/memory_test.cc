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

#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/debugging/leak_check.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/data.h"
#include "common/internal/reference_count.h"
#include "common/native_type.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

#ifdef ABSL_HAVE_EXCEPTIONS
#include <stdexcept>
#endif

namespace cel {
namespace {

// NOLINTBEGIN(bugprone-use-after-move)

using ::testing::_;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NotNull;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;

TEST(MemoryManagement, ostream) {
  {
    std::ostringstream out;
    out << MemoryManagement::kPooling;
    EXPECT_EQ(out.str(), "POOLING");
  }
  {
    std::ostringstream out;
    out << MemoryManagement::kReferenceCounting;
    EXPECT_EQ(out.str(), "REFERENCE_COUNTING");
  }
}

struct TrivialSmallObject {
  uintptr_t ptr;
  char padding[32 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialSmallSizes) {
  google::protobuf::Arena arena;
  MemoryManager memory_manager = MemoryManager::Pooling(&arena);
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialSmallObject>());
  }
}

struct TrivialMediumObject {
  uintptr_t ptr;
  char padding[256 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialMediumSizes) {
  google::protobuf::Arena arena;
  MemoryManager memory_manager = MemoryManager::Pooling(&arena);
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialMediumObject>());
  }
}

struct TrivialLargeObject {
  uintptr_t ptr;
  char padding[4096 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialLargeSizes) {
  google::protobuf::Arena arena;
  MemoryManager memory_manager = MemoryManager::Pooling(&arena);
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialLargeObject>());
  }
}

TEST(RegionalMemoryManager, TrivialMixedSizes) {
  google::protobuf::Arena arena;
  MemoryManager memory_manager = MemoryManager::Pooling(&arena);
  for (size_t i = 0; i < 1024; ++i) {
    switch (i % 3) {
      case 0:
        static_cast<void>(memory_manager.MakeUnique<TrivialSmallObject>());
        break;
      case 1:
        static_cast<void>(memory_manager.MakeUnique<TrivialMediumObject>());
        break;
      case 2:
        static_cast<void>(memory_manager.MakeUnique<TrivialLargeObject>());
        break;
    }
  }
}

struct TrivialHugeObject {
  uintptr_t ptr;
  char padding[32768 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialHugeSizes) {
  google::protobuf::Arena arena;
  MemoryManager memory_manager = MemoryManager::Pooling(&arena);
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialHugeObject>());
  }
}

class SkippableDestructor {
 public:
  explicit SkippableDestructor(bool& deleted) : deleted_(deleted) {}

  ~SkippableDestructor() { deleted_ = true; }

 private:
  bool& deleted_;
};

}  // namespace

template <>
struct NativeTypeTraits<SkippableDestructor> final {
  static bool SkipDestructor(const SkippableDestructor&) { return true; }
};

namespace {

TEST(RegionalMemoryManager, SkippableDestructor) {
  bool deleted = false;
  {
    google::protobuf::Arena arena;
    MemoryManager memory_manager = MemoryManager::Pooling(&arena);
    auto shared = memory_manager.MakeShared<SkippableDestructor>(deleted);
    static_cast<void>(shared);
  }
  EXPECT_FALSE(deleted);
}

class MemoryManagerTest : public TestWithParam<MemoryManagement> {
 public:
  void SetUp() override {}

  void TearDown() override { Finish(); }

  void Finish() { arena_.reset(); }

  MemoryManagerRef memory_manager() {
    switch (memory_management()) {
      case MemoryManagement::kReferenceCounting:
        return MemoryManager::ReferenceCounting();
      case MemoryManagement::kPooling:
        if (!arena_) {
          arena_.emplace();
        }
        return MemoryManager::Pooling(&*arena_);
    }
  }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<google::protobuf::Arena> arena_;
};

TEST_P(MemoryManagerTest, AllocateAndDeallocateZeroSize) {
  EXPECT_THAT(memory_manager().Allocate(0, 1), IsNull());
  EXPECT_THAT(memory_manager().Deallocate(nullptr, 0, 1), IsFalse());
}

TEST_P(MemoryManagerTest, AllocateAndDeallocateBadAlignment) {
  EXPECT_DEBUG_DEATH(absl::IgnoreLeak(memory_manager().Allocate(1, 0)), _);
  EXPECT_DEBUG_DEATH(memory_manager().Deallocate(nullptr, 0, 0), _);
}

TEST_P(MemoryManagerTest, AllocateAndDeallocate) {
  constexpr size_t kSize = 1024;
  constexpr size_t kAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
  void* ptr = memory_manager().Allocate(kSize, kAlignment);
  ASSERT_THAT(ptr, NotNull());
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    EXPECT_THAT(memory_manager().Deallocate(ptr, kSize, kAlignment), IsTrue());
  }
}

TEST_P(MemoryManagerTest, AllocateAndDeallocateOveraligned) {
  constexpr size_t kSize = 1024;
  constexpr size_t kAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__ * 4;
  void* ptr = memory_manager().Allocate(kSize, kAlignment);
  ASSERT_THAT(ptr, NotNull());
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    EXPECT_THAT(memory_manager().Deallocate(ptr, kSize, kAlignment), IsTrue());
  }
}

class Object {
 public:
  Object() : deleted_(nullptr) {}

  explicit Object(bool& deleted) : deleted_(&deleted) {}

  ~Object() {
    if (deleted_ != nullptr) {
      ABSL_CHECK(!*deleted_);
      *deleted_ = true;
    }
  }

  int member = 0;

 private:
  bool* deleted_;
};

class Subobject : public Object {
 public:
  using Object::Object;
};

TEST_P(MemoryManagerTest, Shared) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedAliasCopy) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
    {
      auto member = Shared<int>(object, &object->member);
      EXPECT_TRUE(object);
      EXPECT_FALSE(deleted);
      EXPECT_TRUE(member);
    }
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedAliasMove) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
    {
      auto member = Shared<int>(std::move(object), &object->member);
      EXPECT_FALSE(object);
      EXPECT_FALSE(deleted);
      EXPECT_TRUE(member);
    }
    switch (memory_management()) {
      case MemoryManagement::kPooling:
        EXPECT_FALSE(deleted);
        break;
      case MemoryManagement::kReferenceCounting:
        EXPECT_TRUE(deleted);
        break;
    }
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedStaticCastCopy) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
    {
      auto member = StaticCast<void>(object);
      EXPECT_TRUE(object);
      EXPECT_FALSE(deleted);
      EXPECT_TRUE(member);
    }
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedStaticCastMove) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
    {
      auto member = StaticCast<void>(std::move(object));
      EXPECT_FALSE(object);
      EXPECT_FALSE(deleted);
      EXPECT_TRUE(member);
    }
    switch (memory_management()) {
      case MemoryManagement::kPooling:
        EXPECT_FALSE(deleted);
        break;
      case MemoryManagement::kReferenceCounting:
        EXPECT_TRUE(deleted);
        break;
    }
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyConstruct) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> copied_object(object);
    EXPECT_TRUE(copied_object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveConstruct) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    EXPECT_FALSE(object);
    EXPECT_TRUE(moved_object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyAssign) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    EXPECT_FALSE(object);
    EXPECT_TRUE(moved_object);
    object = moved_object;
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveAssign) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_TRUE(object);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    EXPECT_FALSE(object);
    EXPECT_TRUE(moved_object);
    object = std::move(moved_object);
    EXPECT_FALSE(moved_object);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyConstructConvertible) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Subobject>(deleted);
    EXPECT_TRUE(object);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> copied_object(object);
    EXPECT_TRUE(copied_object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveConstructConvertible) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Subobject>(deleted);
    EXPECT_TRUE(object);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    EXPECT_FALSE(object);
    EXPECT_TRUE(moved_object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyAssignConvertible) {
  bool deleted = false;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    EXPECT_TRUE(subobject);
    auto object = memory_manager().MakeShared<Object>();
    EXPECT_TRUE(object);
    object = subobject;
    EXPECT_TRUE(object);
    EXPECT_TRUE(subobject);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveAssignConvertible) {
  bool deleted = false;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    EXPECT_TRUE(subobject);
    auto object = memory_manager().MakeShared<Object>();
    EXPECT_TRUE(object);
    object = std::move(subobject);
    EXPECT_TRUE(object);
    EXPECT_FALSE(subobject);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedSwap) {
  using std::swap;
  auto object1 = memory_manager().MakeShared<Object>();
  auto object2 = memory_manager().MakeShared<Object>();
  auto* const object1_ptr = object1.operator->();
  auto* const object2_ptr = object2.operator->();
  swap(object1, object2);
  EXPECT_EQ(object1.operator->(), object2_ptr);
  EXPECT_EQ(object2.operator->(), object1_ptr);
}

TEST_P(MemoryManagerTest, SharedPointee) {
  using std::swap;
  auto object = memory_manager().MakeShared<Object>();
  EXPECT_EQ(std::addressof(*object), object.operator->());
}

TEST_P(MemoryManagerTest, SharedViewConstruct) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    dangling_object_view.emplace(object);
    EXPECT_TRUE(*dangling_object_view);
    {
      auto copied_object = Shared<Object>(*dangling_object_view);
      EXPECT_FALSE(deleted);
    }
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyConstruct) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view = SharedView<Object>(object);
    SharedView<Object> copied_object_view(object_view);
    dangling_object_view.emplace(copied_object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveConstruct) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view = SharedView<Object>(object);
    SharedView<Object> moved_object_view(std::move(object_view));
    dangling_object_view.emplace(moved_object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyAssign) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view1 = SharedView<Object>(object);
    SharedView<Object> object_view2(object);
    object_view1 = object_view2;
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveAssign) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view1 = SharedView<Object>(object);
    SharedView<Object> object_view2(object);
    object_view1 = std::move(object_view2);
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyConstructConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto subobject_view = SharedView<Subobject>(subobject);
    SharedView<Object> object_view(subobject_view);
    dangling_object_view.emplace(object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveConstructConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto subobject_view = SharedView<Subobject>(subobject);
    SharedView<Object> object_view(std::move(subobject_view));
    dangling_object_view.emplace(object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyAssignConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto object_view1 = SharedView<Object>(subobject);
    SharedView<Subobject> subobject_view2(subobject);
    object_view1 = subobject_view2;
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveAssignConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto object_view1 = SharedView<Object>(subobject);
    SharedView<Subobject> subobject_view2(subobject);
    object_view1 = std::move(subobject_view2);
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewSwap) {
  using std::swap;
  auto object1 = memory_manager().MakeShared<Object>();
  auto object2 = memory_manager().MakeShared<Object>();
  auto object1_view = SharedView<Object>(object1);
  auto object2_view = SharedView<Object>(object2);
  swap(object1_view, object2_view);
  EXPECT_EQ(object1_view.operator->(), object2.operator->());
  EXPECT_EQ(object2_view.operator->(), object1.operator->());
}

TEST_P(MemoryManagerTest, SharedViewPointee) {
  using std::swap;
  auto object = memory_manager().MakeShared<Object>();
  auto object_view = SharedView<Object>(object);
  EXPECT_EQ(std::addressof(*object_view), object_view.operator->());
}

TEST_P(MemoryManagerTest, Unique) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeUnique<Object>(deleted);
    EXPECT_TRUE(object);
    EXPECT_FALSE(deleted);
  }
  EXPECT_TRUE(deleted);

  Finish();
}

TEST_P(MemoryManagerTest, UniquePointee) {
  using std::swap;
  auto object = memory_manager().MakeUnique<Object>();
  EXPECT_EQ(std::addressof(*object), object.operator->());
}

TEST_P(MemoryManagerTest, UniqueSwap) {
  using std::swap;
  auto object1 = memory_manager().MakeUnique<Object>();
  auto object2 = memory_manager().MakeUnique<Object>();
  auto* const object1_ptr = object1.operator->();
  auto* const object2_ptr = object2.operator->();
  swap(object1, object2);
  EXPECT_EQ(object1.operator->(), object2_ptr);
  EXPECT_EQ(object2.operator->(), object1_ptr);
}

struct EnabledObject : EnableSharedFromThis<EnabledObject> {
  Shared<EnabledObject> This() { return shared_from_this(); }

  Shared<const EnabledObject> This() const { return shared_from_this(); }
};

TEST_P(MemoryManagerTest, EnableSharedFromThis) {
  {
    auto object = memory_manager().MakeShared<EnabledObject>();
    auto this_object = object->This();
    EXPECT_EQ(this_object.operator->(), object.operator->());
  }
  {
    auto object = memory_manager().MakeShared<const EnabledObject>();
    auto this_object = object->This();
    EXPECT_EQ(this_object.operator->(), object.operator->());
  }
  Finish();
}

struct ThrowingConstructorObject {
  ThrowingConstructorObject() {
#ifdef ABSL_HAVE_EXCEPTIONS
    throw std::invalid_argument("ThrowingConstructorObject");
#endif
  }

  char padding[64];
};

TEST_P(MemoryManagerTest, SharedThrowingConstructor) {
#ifdef ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(static_cast<void>(
                   memory_manager().MakeShared<ThrowingConstructorObject>()),
               std::invalid_argument);
#else
  GTEST_SKIP();
#endif
}

TEST_P(MemoryManagerTest, UniqueThrowingConstructor) {
#ifdef ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(static_cast<void>(
                   memory_manager().MakeUnique<ThrowingConstructorObject>()),
               std::invalid_argument);
#else
  GTEST_SKIP();
#endif
}

INSTANTIATE_TEST_SUITE_P(
    MemoryManagerTest, MemoryManagerTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MemoryManagerTest::ToString);

// NOLINTEND(bugprone-use-after-move)

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

class OwnedTest : public TestWithParam<MemoryManagement> {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case MemoryManagement::kPooling:
        return ArenaAllocator<>{&arena_};
      case MemoryManagement::kReferenceCounting:
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

  explicit TestData(absl::Nullable<google::protobuf::Arena*> arena) noexcept
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

INSTANTIATE_TEST_SUITE_P(
    OwnedTest, OwnedTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting));

class BorrowedTest : public TestWithParam<MemoryManagement> {
 public:
  Allocator<> GetAllocator() {
    switch (GetParam()) {
      case MemoryManagement::kPooling:
        return ArenaAllocator<>{&arena_};
      case MemoryManagement::kReferenceCounting:
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

INSTANTIATE_TEST_SUITE_P(
    BorrowedTest, BorrowedTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting));

}  // namespace
}  // namespace cel
