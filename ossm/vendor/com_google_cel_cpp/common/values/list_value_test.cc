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

#include <cstdint>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::testing::ElementsAreArray;
using ::testing::TestParamInfo;

class ListValueTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<ListValue> NewIntListValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_manager().NewListValueBuilder(ListType()));
    (static_cast<void>(builder->Add(std::forward<Args>(args))), ...);
    return std::move(*builder).Build();
  }
};

TEST_P(ListValueTest, Default) {
  ListValue value;
  EXPECT_THAT(value.IsEmpty(), IsOkAndHolds(true));
  EXPECT_THAT(value.Size(), IsOkAndHolds(0));
  EXPECT_EQ(value.DebugString(), "[]");
}

TEST_P(ListValueTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(value.kind(), ListValue::kKind);
  EXPECT_EQ(Value(value).kind(), ListValue::kKind);
}

TEST_P(ListValueTest, Type) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
}

TEST_P(ListValueTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "[0, 1, 2]");
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_EQ(out.str(), "[0, 1, 2]");
  }
}

TEST_P(ListValueTest, IsEmpty) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(value.IsEmpty(), IsOkAndHolds(false));
}

TEST_P(ListValueTest, Size) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(value.Size(), IsOkAndHolds(3));
}

TEST_P(ListValueTest, Get) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto element, value.Get(value_manager(), 0));
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  ASSERT_EQ(Cast<IntValue>(element).NativeValue(), 0);
  ASSERT_OK_AND_ASSIGN(element, value.Get(value_manager(), 1));
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  ASSERT_EQ(Cast<IntValue>(element).NativeValue(), 1);
  ASSERT_OK_AND_ASSIGN(element, value.Get(value_manager(), 2));
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  ASSERT_EQ(Cast<IntValue>(element).NativeValue(), 2);
  EXPECT_THAT(
      value.Get(value_manager(), 3),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_P(ListValueTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  std::vector<int64_t> elements;
  EXPECT_OK(value.ForEach(value_manager(), [&elements](const Value& element) {
    elements.push_back(Cast<IntValue>(element).NativeValue());
    return true;
  }));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueTest, Contains) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto contained,
                       value.Contains(value_manager(), IntValue(2)));
  ASSERT_TRUE(InstanceOf<BoolValue>(contained));
  EXPECT_TRUE(Cast<BoolValue>(contained).NativeValue());
  ASSERT_OK_AND_ASSIGN(contained, value.Contains(value_manager(), IntValue(3)));
  ASSERT_TRUE(InstanceOf<BoolValue>(contained));
  EXPECT_FALSE(Cast<BoolValue>(contained).NativeValue());
}

TEST_P(ListValueTest, NewIterator) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(value_manager()));
  std::vector<int64_t> elements;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto element, iterator->Next(value_manager()));
    ASSERT_TRUE(InstanceOf<IntValue>(element));
    elements.push_back(Cast<IntValue>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              IsOkAndHolds(Json(MakeJsonArray({0.0, 1.0, 2.0}))));
}

INSTANTIATE_TEST_SUITE_P(
    ListValueTest, ListValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    ListValueTest::ToString);

}  // namespace
}  // namespace cel
