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
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::testing::ElementsAreArray;

class ListValueTest : public common_internal::ValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<ListValue> NewIntListValue(Args&&... args) {
    auto builder = NewListValueBuilder(arena());
    (static_cast<void>(builder->Add(std::forward<Args>(args))), ...);
    return std::move(*builder).Build();
  }
};

TEST_F(ListValueTest, Default) {
  ListValue value;
  EXPECT_THAT(value.IsEmpty(), IsOkAndHolds(true));
  EXPECT_THAT(value.Size(), IsOkAndHolds(0));
  EXPECT_EQ(value.DebugString(), "[]");
}

TEST_F(ListValueTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(value.kind(), ListValue::kKind);
  EXPECT_EQ(Value(value).kind(), ListValue::kKind);
}

TEST_F(ListValueTest, DebugString) {
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

TEST_F(ListValueTest, IsEmpty) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(value.IsEmpty(), IsOkAndHolds(false));
}

TEST_F(ListValueTest, Size) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(value.Size(), IsOkAndHolds(3));
}

TEST_F(ListValueTest, Get) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto element, value.Get(0, descriptor_pool(),
                                               message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  ASSERT_EQ(Cast<IntValue>(element).NativeValue(), 0);
  ASSERT_OK_AND_ASSIGN(
      element, value.Get(1, descriptor_pool(), message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  ASSERT_EQ(Cast<IntValue>(element).NativeValue(), 1);
  ASSERT_OK_AND_ASSIGN(
      element, value.Get(2, descriptor_pool(), message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  ASSERT_EQ(Cast<IntValue>(element).NativeValue(), 2);
  EXPECT_THAT(
      value.Get(3, descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_F(ListValueTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  std::vector<int64_t> elements;
  EXPECT_THAT(value.ForEach(
                  [&elements](const Value& element) {
                    elements.push_back(Cast<IntValue>(element).NativeValue());
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_F(ListValueTest, Contains) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto contained,
                       value.Contains(IntValue(2), descriptor_pool(),
                                      message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<BoolValue>(contained));
  EXPECT_TRUE(Cast<BoolValue>(contained).NativeValue());
  ASSERT_OK_AND_ASSIGN(contained, value.Contains(IntValue(3), descriptor_pool(),
                                                 message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<BoolValue>(contained));
  EXPECT_FALSE(Cast<BoolValue>(contained).NativeValue());
}

TEST_F(ListValueTest, NewIterator) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  std::vector<int64_t> elements;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto element,
        iterator->Next(descriptor_pool(), message_factory(), arena()));
    ASSERT_TRUE(InstanceOf<IntValue>(element));
    elements.push_back(Cast<IntValue>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_F(ListValueTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(
      value.ConvertToJson(descriptor_pool(), message_factory(), message),
      IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(list_value: {
                                                    values: { number_value: 0 }
                                                    values: { number_value: 1 }
                                                    values: { number_value: 2 }
                                                  })pb"));
}

}  // namespace
}  // namespace cel
