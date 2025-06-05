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

#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/value_testing.h"
#include "common/values/map_value_builder.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::ListValueElements;
using ::cel::test::ListValueIs;
using ::cel::test::StringValueIs;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pair;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

class MutableMapValueTest : public TestWithParam<AllocatorKind> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        arena_.emplace();
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::Pooling(arena()),
            NewThreadCompatibleTypeReflector(MemoryManager::Pooling(arena())));
        break;
      case AllocatorKind::kNewDelete:
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::ReferenceCounting(),
            NewThreadCompatibleTypeReflector(
                MemoryManager::ReferenceCounting()));
        break;
    }
  }

  void TearDown() override {
    value_manager_.reset();
    arena_.reset();
  }

  Allocator<> allocator() {
    return arena_ ? Allocator(ArenaAllocator<>{&*arena_})
                  : Allocator(NewDeleteAllocator<>{});
  }

  absl::Nullable<google::protobuf::Arena*> arena() { return allocator().arena(); }

  ValueManager& value_manager() { return **value_manager_; }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(MutableMapValueTest, DebugString) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  EXPECT_THAT(mutable_map_value->DebugString(), "{}");
}

TEST_P(MutableMapValueTest, IsEmpty) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  EXPECT_TRUE(mutable_map_value->IsEmpty());
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_FALSE(mutable_map_value->IsEmpty());
}

TEST_P(MutableMapValueTest, Size) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  EXPECT_THAT(mutable_map_value->Size(), 0);
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->Size(), 1);
}

TEST_P(MutableMapValueTest, ConvertToJson) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  EXPECT_THAT(mutable_map_value->ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonObject>(JsonObject())));
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonObject>(
                  MakeJsonObject({{JsonString("foo"), JsonInt(1)}}))));
}

TEST_P(MutableMapValueTest, ListKeys) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  ListValue keys;
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->ListKeys(value_manager(), keys), IsOk());
  EXPECT_THAT(
      keys, ListValueIs(ListValueElements(
                &value_manager(), UnorderedElementsAre(StringValueIs("foo")))));
}

TEST_P(MutableMapValueTest, ForEach) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  std::vector<std::pair<Value, Value>> entries;
  auto for_each_callback = [&](const Value& key,
                               const Value& value) -> absl::StatusOr<bool> {
    entries.push_back(std::pair{key, value});
    return true;
  };
  EXPECT_THAT(mutable_map_value->ForEach(value_manager(), for_each_callback),
              IsOk());
  EXPECT_THAT(entries, IsEmpty());
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->ForEach(value_manager(), for_each_callback),
              IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(StringValueIs("foo"), IntValueIs(1))));
}

TEST_P(MutableMapValueTest, NewIterator) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       mutable_map_value->NewIterator(value_manager()));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  ASSERT_OK_AND_ASSIGN(iterator,
                       mutable_map_value->NewIterator(value_manager()));
  EXPECT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              IsOkAndHolds(StringValueIs("foo")));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MutableMapValueTest, FindHas) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  mutable_map_value->Reserve(1);
  Value value;
  EXPECT_THAT(
      mutable_map_value->Find(value_manager(), StringValue("foo"), value),
      IsOkAndHolds(IsFalse()));
  EXPECT_THAT(value, IsNullValue());
  EXPECT_THAT(
      mutable_map_value->Has(value_manager(), StringValue("foo"), value),
      IsOk());
  EXPECT_THAT(value, BoolValueIs(false));
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(
      mutable_map_value->Find(value_manager(), StringValue("foo"), value),
      IsOkAndHolds(IsTrue()));
  EXPECT_THAT(value, IntValueIs(1));
  EXPECT_THAT(
      mutable_map_value->Has(value_manager(), StringValue("foo"), value),
      IsOk());
  EXPECT_THAT(value, BoolValueIs(true));
}

TEST_P(MutableMapValueTest, IsMutableMapValue) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  EXPECT_TRUE(IsMutableMapValue(Value(ParsedMapValue(mutable_map_value))));
  EXPECT_TRUE(IsMutableMapValue(MapValue(ParsedMapValue(mutable_map_value))));
}

TEST_P(MutableMapValueTest, AsMutableMapValue) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  EXPECT_EQ(AsMutableMapValue(Value(ParsedMapValue(mutable_map_value))),
            mutable_map_value.operator->());
  EXPECT_EQ(AsMutableMapValue(MapValue(ParsedMapValue(mutable_map_value))),
            mutable_map_value.operator->());
}

TEST_P(MutableMapValueTest, GetMutableMapValue) {
  auto mutable_map_value = NewMutableMapValue(allocator());
  EXPECT_EQ(&GetMutableMapValue(Value(ParsedMapValue(mutable_map_value))),
            mutable_map_value.operator->());
  EXPECT_EQ(&GetMutableMapValue(MapValue(ParsedMapValue(mutable_map_value))),
            mutable_map_value.operator->());
}

INSTANTIATE_TEST_SUITE_P(MutableMapValueTest, MutableMapValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel::common_internal
