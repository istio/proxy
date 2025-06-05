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

#include <cstddef>
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
#include "common/values/list_value_builder.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::StringValueIs;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

class MutableListValueTest : public TestWithParam<AllocatorKind> {
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

TEST_P(MutableListValueTest, DebugString) {
  auto mutable_list_value = NewMutableListValue(allocator());
  EXPECT_THAT(mutable_list_value->DebugString(), "[]");
}

TEST_P(MutableListValueTest, IsEmpty) {
  auto mutable_list_value = NewMutableListValue(allocator());
  mutable_list_value->Reserve(1);
  EXPECT_TRUE(mutable_list_value->IsEmpty());
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_FALSE(mutable_list_value->IsEmpty());
}

TEST_P(MutableListValueTest, Size) {
  auto mutable_list_value = NewMutableListValue(allocator());
  mutable_list_value->Reserve(1);
  EXPECT_THAT(mutable_list_value->Size(), 0);
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(mutable_list_value->Size(), 1);
}

TEST_P(MutableListValueTest, ConvertToJson) {
  auto mutable_list_value = NewMutableListValue(allocator());
  mutable_list_value->Reserve(1);
  EXPECT_THAT(mutable_list_value->ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonArray>(JsonArray())));
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(
      mutable_list_value->ConvertToJson(value_manager()),
      IsOkAndHolds(VariantWith<JsonArray>(MakeJsonArray({JsonString("foo")}))));
}

TEST_P(MutableListValueTest, ForEach) {
  auto mutable_list_value = NewMutableListValue(allocator());
  mutable_list_value->Reserve(1);
  std::vector<std::pair<size_t, Value>> elements;
  auto for_each_callback = [&](size_t index,
                               const Value& value) -> absl::StatusOr<bool> {
    elements.push_back(std::pair{index, value});
    return true;
  };
  EXPECT_THAT(mutable_list_value->ForEach(value_manager(), for_each_callback),
              IsOk());
  EXPECT_THAT(elements, IsEmpty());
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(mutable_list_value->ForEach(value_manager(), for_each_callback),
              IsOk());
  EXPECT_THAT(elements, UnorderedElementsAre(Pair(0, StringValueIs("foo"))));
}

TEST_P(MutableListValueTest, NewIterator) {
  auto mutable_list_value = NewMutableListValue(allocator());
  mutable_list_value->Reserve(1);
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       mutable_list_value->NewIterator(value_manager()));
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  ASSERT_OK_AND_ASSIGN(iterator,
                       mutable_list_value->NewIterator(value_manager()));
  EXPECT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              IsOkAndHolds(StringValueIs("foo")));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MutableListValueTest, Get) {
  auto mutable_list_value = NewMutableListValue(allocator());
  mutable_list_value->Reserve(1);
  Value value;
  EXPECT_THAT(mutable_list_value->Get(value_manager(), 0, value), IsOk());
  EXPECT_THAT(value,
              ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument)));
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(mutable_list_value->Get(value_manager(), 0, value), IsOk());
  EXPECT_THAT(value, StringValueIs("foo"));
}

TEST_P(MutableListValueTest, IsMutablListValue) {
  auto mutable_list_value = NewMutableListValue(allocator());
  EXPECT_TRUE(IsMutableListValue(Value(ParsedListValue(mutable_list_value))));
  EXPECT_TRUE(
      IsMutableListValue(ListValue(ParsedListValue(mutable_list_value))));
}

TEST_P(MutableListValueTest, AsMutableListValue) {
  auto mutable_list_value = NewMutableListValue(allocator());
  EXPECT_EQ(AsMutableListValue(Value(ParsedListValue(mutable_list_value))),
            mutable_list_value.operator->());
  EXPECT_EQ(AsMutableListValue(ListValue(ParsedListValue(mutable_list_value))),
            mutable_list_value.operator->());
}

TEST_P(MutableListValueTest, GetMutableListValue) {
  auto mutable_list_value = NewMutableListValue(allocator());
  EXPECT_EQ(&GetMutableListValue(Value(ParsedListValue(mutable_list_value))),
            mutable_list_value.operator->());
  EXPECT_EQ(
      &GetMutableListValue(ListValue(ParsedListValue(mutable_list_value))),
      mutable_list_value.operator->());
}

INSTANTIATE_TEST_SUITE_P(MutableListValueTest, MutableListValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel::common_internal
