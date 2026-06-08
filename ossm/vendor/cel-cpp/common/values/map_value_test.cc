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
#include "absl/types/optional.h"
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
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

TEST(MapValue, CheckKey) {
  EXPECT_THAT(CheckMapKey(BoolValue()), IsOk());
  EXPECT_THAT(CheckMapKey(IntValue()), IsOk());
  EXPECT_THAT(CheckMapKey(UintValue()), IsOk());
  EXPECT_THAT(CheckMapKey(StringValue()), IsOk());
  EXPECT_THAT(CheckMapKey(BytesValue()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

class MapValueTest : public common_internal::ValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<MapValue> NewIntDoubleMapValue(Args&&... args) {
    auto builder = NewMapValueBuilder(arena());
    (static_cast<void>(builder->Put(std::forward<Args>(args).first,
                                    std::forward<Args>(args).second)),
     ...);
    return std::move(*builder).Build();
  }

  template <typename... Args>
  absl::StatusOr<MapValue> NewJsonMapValue(Args&&... args) {
    auto builder = NewMapValueBuilder(arena());
    (static_cast<void>(builder->Put(std::forward<Args>(args).first,
                                    std::forward<Args>(args).second)),
     ...);
    return std::move(*builder).Build();
  }
};

TEST_F(MapValueTest, Default) {
  MapValue map_value;
  EXPECT_THAT(map_value.IsEmpty(), IsOkAndHolds(true));
  EXPECT_THAT(map_value.Size(), IsOkAndHolds(0));
  EXPECT_EQ(map_value.DebugString(), "{}");
  ASSERT_OK_AND_ASSIGN(
      auto list_value,
      map_value.ListKeys(descriptor_pool(), message_factory(), arena()));
  EXPECT_THAT(list_value.IsEmpty(), IsOkAndHolds(true));
  EXPECT_THAT(list_value.Size(), IsOkAndHolds(0));
  EXPECT_EQ(list_value.DebugString(), "[]");
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value.NewIterator());
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(MapValueTest, Kind) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(value.kind(), MapValue::kKind);
  EXPECT_EQ(Value(value).kind(), MapValue::kKind);
}

TEST_F(MapValueTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  {
    std::ostringstream out;
    out << value;
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
}

TEST_F(MapValueTest, IsEmpty) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_THAT(value.IsEmpty(), IsOkAndHolds(false));
}

TEST_F(MapValueTest, Size) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_THAT(value.Size(), IsOkAndHolds(3));
}

TEST_F(MapValueTest, Get) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto value, map_value.Get(IntValue(0), descriptor_pool(),
                                                 message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(value, map_value.Get(IntValue(1), descriptor_pool(),
                                            message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(value, map_value.Get(IntValue(2), descriptor_pool(),
                                            message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 5.0);
  EXPECT_THAT(
      map_value.Get(IntValue(3), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_F(MapValueTest, Find) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  absl::optional<Value> entry;
  ASSERT_OK_AND_ASSIGN(entry, map_value.Find(IntValue(0), descriptor_pool(),
                                             message_factory(), arena()));
  ASSERT_TRUE(entry);
  ASSERT_TRUE(InstanceOf<DoubleValue>(*entry));
  ASSERT_EQ(Cast<DoubleValue>(*entry).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(entry, map_value.Find(IntValue(1), descriptor_pool(),
                                             message_factory(), arena()));
  ASSERT_TRUE(entry);
  ASSERT_TRUE(InstanceOf<DoubleValue>(*entry));
  ASSERT_EQ(Cast<DoubleValue>(*entry).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(entry, map_value.Find(IntValue(2), descriptor_pool(),
                                             message_factory(), arena()));
  ASSERT_TRUE(entry);
  ASSERT_TRUE(InstanceOf<DoubleValue>(*entry));
  ASSERT_EQ(Cast<DoubleValue>(*entry).NativeValue(), 5.0);
  ASSERT_OK_AND_ASSIGN(entry, map_value.Find(IntValue(3), descriptor_pool(),
                                             message_factory(), arena()));
  ASSERT_FALSE(entry);
}

TEST_F(MapValueTest, Has) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto value, map_value.Has(IntValue(0), descriptor_pool(),
                                                 message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_TRUE(Cast<BoolValue>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(IntValue(1), descriptor_pool(),
                                            message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_TRUE(Cast<BoolValue>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(IntValue(2), descriptor_pool(),
                                            message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_TRUE(Cast<BoolValue>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(IntValue(3), descriptor_pool(),
                                            message_factory(), arena()));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_FALSE(Cast<BoolValue>(value).NativeValue());
}

TEST_F(MapValueTest, ListKeys) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(
      auto list_keys,
      map_value.ListKeys(descriptor_pool(), message_factory(), arena()));
  std::vector<int64_t> keys;
  ASSERT_THAT(list_keys.ForEach(
                  [&keys](const Value& element) -> bool {
                    keys.push_back(Cast<IntValue>(element).NativeValue());
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_F(MapValueTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  std::vector<std::pair<int64_t, double>> entries;
  EXPECT_THAT(value.ForEach(
                  [&entries](const Value& key, const Value& value) {
                    entries.push_back(
                        std::pair{Cast<IntValue>(key).NativeValue(),
                                  Cast<DoubleValue>(value).NativeValue()});
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAreArray(
                  {std::pair{0, 3.0}, std::pair{1, 4.0}, std::pair{2, 5.0}}));
}

TEST_F(MapValueTest, NewIterator) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  std::vector<int64_t> keys;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto element,
        iterator->Next(descriptor_pool(), message_factory(), arena()));
    ASSERT_TRUE(InstanceOf<IntValue>(element));
    keys.push_back(Cast<IntValue>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_F(MapValueTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewJsonMapValue(std::pair{StringValue("0"), DoubleValue(3.0)},
                      std::pair{StringValue("1"), DoubleValue(4.0)},
                      std::pair{StringValue("2"), DoubleValue(5.0)}));
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(
      value.ConvertToJson(descriptor_pool(), message_factory(), message),
      IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(struct_value: {
                                                    fields: {
                                                      key: "0"
                                                      value: { number_value: 3 }
                                                    }
                                                    fields: {
                                                      key: "1"
                                                      value: { number_value: 4 }
                                                    }
                                                    fields: {
                                                      key: "2"
                                                      value: { number_value: 5 }
                                                    }
                                                  })pb"));
}

}  // namespace
}  // namespace cel
