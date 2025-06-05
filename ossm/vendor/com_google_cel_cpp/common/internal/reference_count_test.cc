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

#include "common/internal/reference_count.h"

#include <tuple>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "common/data.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message_lite.h"

namespace cel::common_internal {
namespace {

using ::testing::NotNull;
using ::testing::WhenDynamicCastTo;

class Object : public virtual ReferenceCountFromThis {
 public:
  explicit Object(bool& destructed) : destructed_(destructed) {}

  ~Object() { destructed_ = true; }

 private:
  bool& destructed_;
};

class Subobject : public Object, public virtual ReferenceCountFromThis {
 public:
  using Object::Object;
};

TEST(ReferenceCount, Strong) {
  bool destructed = false;
  Object* object;
  ReferenceCount* refcount;
  std::tie(object, refcount) = MakeReferenceCount<Subobject>(destructed);
  EXPECT_EQ(GetReferenceCountForThat(*object), refcount);
  EXPECT_EQ(GetReferenceCountForThat(*static_cast<Subobject*>(object)),
            refcount);
  StrongRef(refcount);
  StrongUnref(refcount);
  EXPECT_TRUE(IsUniqueRef(refcount));
  EXPECT_FALSE(IsExpiredRef(refcount));
  EXPECT_FALSE(destructed);
  StrongUnref(refcount);
  EXPECT_TRUE(destructed);
}

TEST(ReferenceCount, Weak) {
  bool destructed = false;
  Object* object;
  ReferenceCount* refcount;
  std::tie(object, refcount) = MakeReferenceCount<Subobject>(destructed);
  EXPECT_EQ(GetReferenceCountForThat(*object), refcount);
  EXPECT_EQ(GetReferenceCountForThat(*static_cast<Subobject*>(object)),
            refcount);
  WeakRef(refcount);
  ASSERT_TRUE(StrengthenRef(refcount));
  StrongUnref(refcount);
  EXPECT_TRUE(IsUniqueRef(refcount));
  EXPECT_FALSE(IsExpiredRef(refcount));
  EXPECT_FALSE(destructed);
  StrongUnref(refcount);
  EXPECT_TRUE(destructed);
  EXPECT_TRUE(IsExpiredRef(refcount));
  ASSERT_FALSE(StrengthenRef(refcount));
  WeakUnref(refcount);
}

class DataObject final : public Data {
 public:
  DataObject() noexcept : Data() {}

  explicit DataObject(absl::Nullable<google::protobuf::Arena*> arena) noexcept
      : Data(arena) {}

  char member_[17];
};

struct OtherObject final {
  char data[17];
};

TEST(DeletingReferenceCount, Data) {
  auto* data = new DataObject();
  const auto* refcount = MakeDeletingReferenceCount(data);
  EXPECT_THAT(refcount, WhenDynamicCastTo<const DeletingReferenceCount<Data>*>(
                            NotNull()));
  EXPECT_EQ(common_internal::GetDataReferenceCount(data), refcount);
  StrongUnref(refcount);
}

TEST(DeletingReferenceCount, MessageLite) {
  auto* message_lite = new google::protobuf::Value();
  const auto* refcount = MakeDeletingReferenceCount(message_lite);
  EXPECT_THAT(
      refcount,
      WhenDynamicCastTo<const DeletingReferenceCount<google::protobuf::MessageLite>*>(
          NotNull()));
  StrongUnref(refcount);
}

TEST(DeletingReferenceCount, Other) {
  auto* other = new OtherObject();
  const auto* refcount = MakeDeletingReferenceCount(other);
  EXPECT_THAT(
      refcount,
      WhenDynamicCastTo<const DeletingReferenceCount<OtherObject>*>(NotNull()));
  StrongUnref(refcount);
}

TEST(EmplacedReferenceCount, Data) {
  Data* data;
  const ReferenceCount* refcount;
  std::tie(data, refcount) = MakeEmplacedReferenceCount<DataObject>();
  EXPECT_THAT(
      refcount,
      WhenDynamicCastTo<const EmplacedReferenceCount<DataObject>*>(NotNull()));
  EXPECT_EQ(common_internal::GetDataReferenceCount(data), refcount);
  StrongUnref(refcount);
}

TEST(EmplacedReferenceCount, MessageLite) {
  google::protobuf::Value* message_lite;
  const ReferenceCount* refcount;
  std::tie(message_lite, refcount) =
      MakeEmplacedReferenceCount<google::protobuf::Value>();
  EXPECT_THAT(
      refcount,
      WhenDynamicCastTo<const EmplacedReferenceCount<google::protobuf::Value>*>(
          NotNull()));
  StrongUnref(refcount);
}

TEST(EmplacedReferenceCount, Other) {
  OtherObject* other;
  const ReferenceCount* refcount;
  std::tie(other, refcount) = MakeEmplacedReferenceCount<OtherObject>();
  EXPECT_THAT(
      refcount,
      WhenDynamicCastTo<const EmplacedReferenceCount<OtherObject>*>(NotNull()));
  StrongUnref(refcount);
}

}  // namespace
}  // namespace cel::common_internal
