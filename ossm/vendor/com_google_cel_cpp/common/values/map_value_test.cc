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
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
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

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::TestParamInfo;
using ::testing::UnorderedElementsAreArray;

TEST(MapValue, CheckKey) {
  EXPECT_THAT(CheckMapKey(BoolValue()), IsOk());
  EXPECT_THAT(CheckMapKey(IntValue()), IsOk());
  EXPECT_THAT(CheckMapKey(UintValue()), IsOk());
  EXPECT_THAT(CheckMapKey(StringValue()), IsOk());
  EXPECT_THAT(CheckMapKey(BytesValue()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

class MapValueTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<MapValue> NewIntDoubleMapValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_manager().NewMapValueBuilder(MapType()));
    (static_cast<void>(builder->Put(std::forward<Args>(args).first,
                                    std::forward<Args>(args).second)),
     ...);
    return std::move(*builder).Build();
  }

  template <typename... Args>
  absl::StatusOr<MapValue> NewJsonMapValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_manager().NewMapValueBuilder(JsonMapType()));
    (static_cast<void>(builder->Put(std::forward<Args>(args).first,
                                    std::forward<Args>(args).second)),
     ...);
    return std::move(*builder).Build();
  }
};

TEST_P(MapValueTest, Default) {
  MapValue map_value;
  EXPECT_THAT(map_value.IsEmpty(), IsOkAndHolds(true));
  EXPECT_THAT(map_value.Size(), IsOkAndHolds(0));
  EXPECT_EQ(map_value.DebugString(), "{}");
  ASSERT_OK_AND_ASSIGN(auto list_value, map_value.ListKeys(value_manager()));
  EXPECT_THAT(list_value.IsEmpty(), IsOkAndHolds(true));
  EXPECT_THAT(list_value.Size(), IsOkAndHolds(0));
  EXPECT_EQ(list_value.DebugString(), "[]");
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value.NewIterator(value_manager()));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MapValueTest, Kind) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(value.kind(), MapValue::kKind);
  EXPECT_EQ(Value(value).kind(), MapValue::kKind);
}

TEST_P(MapValueTest, DebugString) {
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

TEST_P(MapValueTest, IsEmpty) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_THAT(value.IsEmpty(), IsOkAndHolds(false));
}

TEST_P(MapValueTest, Size) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_THAT(value.Size(), IsOkAndHolds(3));
}

TEST_P(MapValueTest, Get) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto value, map_value.Get(value_manager(), IntValue(0)));
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(value, map_value.Get(value_manager(), IntValue(1)));
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(value, map_value.Get(value_manager(), IntValue(2)));
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 5.0);
  EXPECT_THAT(
      map_value.Get(value_manager(), IntValue(3)),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_P(MapValueTest, Find) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  Value value;
  bool ok;
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(value_manager(), IntValue(0)));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(value_manager(), IntValue(1)));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(value_manager(), IntValue(2)));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  ASSERT_EQ(Cast<DoubleValue>(value).NativeValue(), 5.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(value_manager(), IntValue(3)));
  ASSERT_FALSE(ok);
}

TEST_P(MapValueTest, Has) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto value, map_value.Has(value_manager(), IntValue(0)));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_TRUE(Cast<BoolValue>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(value_manager(), IntValue(1)));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_TRUE(Cast<BoolValue>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(value_manager(), IntValue(2)));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_TRUE(Cast<BoolValue>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(value_manager(), IntValue(3)));
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  ASSERT_FALSE(Cast<BoolValue>(value).NativeValue());
}

TEST_P(MapValueTest, ListKeys) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto list_keys, map_value.ListKeys(value_manager()));
  std::vector<int64_t> keys;
  ASSERT_OK(
      list_keys.ForEach(value_manager(), [&keys](const Value& element) -> bool {
        keys.push_back(Cast<IntValue>(element).NativeValue());
        return true;
      }));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_P(MapValueTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  std::vector<std::pair<int64_t, double>> entries;
  EXPECT_OK(value.ForEach(
      value_manager(), [&entries](const Value& key, const Value& value) {
        entries.push_back(std::pair{Cast<IntValue>(key).NativeValue(),
                                    Cast<DoubleValue>(value).NativeValue()});
        return true;
      }));
  EXPECT_THAT(entries,
              UnorderedElementsAreArray(
                  {std::pair{0, 3.0}, std::pair{1, 4.0}, std::pair{2, 5.0}}));
}

TEST_P(MapValueTest, NewIterator) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(value_manager()));
  std::vector<int64_t> keys;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto element, iterator->Next(value_manager()));
    ASSERT_TRUE(InstanceOf<IntValue>(element));
    keys.push_back(Cast<IntValue>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_P(MapValueTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewJsonMapValue(std::pair{StringValue("0"), DoubleValue(3.0)},
                      std::pair{StringValue("1"), DoubleValue(4.0)},
                      std::pair{StringValue("2"), DoubleValue(5.0)}));
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              IsOkAndHolds(Json(MakeJsonObject({{JsonString("0"), 3.0},
                                                {JsonString("1"), 4.0},
                                                {JsonString("2"), 5.0}}))));
}

INSTANTIATE_TEST_SUITE_P(
    MapValueTest, MapValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapValueTest::ToString);

}  // namespace
}  // namespace cel
