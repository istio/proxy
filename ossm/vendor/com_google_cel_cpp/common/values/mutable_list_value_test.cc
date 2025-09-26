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

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "common/values/list_value_builder.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::StringValueIs;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using MutableListValueTest = common_internal::ValueTest<>;

TEST_F(MutableListValueTest, DebugString) {
  auto* mutable_list_value = NewMutableListValue(arena());
  EXPECT_THAT(CustomListValue(mutable_list_value, arena()).DebugString(), "[]");
}

TEST_F(MutableListValueTest, IsEmpty) {
  auto* mutable_list_value = NewMutableListValue(arena());
  mutable_list_value->Reserve(1);
  EXPECT_TRUE(CustomListValue(mutable_list_value, arena()).IsEmpty());
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_FALSE(CustomListValue(mutable_list_value, arena()).IsEmpty());
}

TEST_F(MutableListValueTest, Size) {
  auto* mutable_list_value = NewMutableListValue(arena());
  mutable_list_value->Reserve(1);
  EXPECT_THAT(CustomListValue(mutable_list_value, arena()).Size(), 0);
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(CustomListValue(mutable_list_value, arena()).Size(), 1);
}

TEST_F(MutableListValueTest, ForEach) {
  auto* mutable_list_value = NewMutableListValue(arena());
  mutable_list_value->Reserve(1);
  std::vector<std::pair<size_t, Value>> elements;
  auto for_each_callback = [&](size_t index,
                               const Value& value) -> absl::StatusOr<bool> {
    elements.push_back(std::pair{index, value});
    return true;
  };
  EXPECT_THAT(CustomListValue(mutable_list_value, arena())
                  .ForEach(for_each_callback, descriptor_pool(),
                           message_factory(), arena()),
              IsOk());
  EXPECT_THAT(elements, IsEmpty());
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(CustomListValue(mutable_list_value, arena())
                  .ForEach(for_each_callback, descriptor_pool(),
                           message_factory(), arena()),
              IsOk());
  EXPECT_THAT(elements, UnorderedElementsAre(Pair(0, StringValueIs("foo"))));
}

TEST_F(MutableListValueTest, NewIterator) {
  auto* mutable_list_value = NewMutableListValue(arena());
  mutable_list_value->Reserve(1);
  ASSERT_OK_AND_ASSIGN(
      auto iterator,
      CustomListValue(mutable_list_value, arena()).NewIterator());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  ASSERT_OK_AND_ASSIGN(
      iterator, CustomListValue(mutable_list_value, arena()).NewIterator());
  EXPECT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(StringValueIs("foo")));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(MutableListValueTest, Get) {
  auto* mutable_list_value = NewMutableListValue(arena());
  mutable_list_value->Reserve(1);
  Value value;
  EXPECT_THAT(
      CustomListValue(mutable_list_value, arena())
          .Get(0, descriptor_pool(), message_factory(), arena(), &value),
      IsOk());
  EXPECT_THAT(value,
              ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument)));
  EXPECT_THAT(mutable_list_value->Append(StringValue("foo")), IsOk());
  EXPECT_THAT(
      CustomListValue(mutable_list_value, arena())
          .Get(0, descriptor_pool(), message_factory(), arena(), &value),
      IsOk());
  EXPECT_THAT(value, StringValueIs("foo"));
}

TEST_F(MutableListValueTest, IsMutablListValue) {
  auto* mutable_list_value = NewMutableListValue(arena());
  EXPECT_TRUE(
      IsMutableListValue(Value(CustomListValue(mutable_list_value, arena()))));
  EXPECT_TRUE(IsMutableListValue(
      ListValue(CustomListValue(mutable_list_value, arena()))));
}

TEST_F(MutableListValueTest, AsMutableListValue) {
  auto* mutable_list_value = NewMutableListValue(arena());
  EXPECT_EQ(
      AsMutableListValue(Value(CustomListValue(mutable_list_value, arena()))),
      mutable_list_value);
  EXPECT_EQ(AsMutableListValue(
                ListValue(CustomListValue(mutable_list_value, arena()))),
            mutable_list_value);
}

TEST_F(MutableListValueTest, GetMutableListValue) {
  auto* mutable_list_value = NewMutableListValue(arena());
  EXPECT_EQ(
      &GetMutableListValue(Value(CustomListValue(mutable_list_value, arena()))),
      mutable_list_value);
  EXPECT_EQ(&GetMutableListValue(
                ListValue(CustomListValue(mutable_list_value, arena()))),
            mutable_list_value);
}

}  // namespace
}  // namespace cel::common_internal
