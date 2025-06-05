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

#include "common/value_factory.h"

#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::TestParamInfo;
using ::testing::UnorderedElementsAreArray;

class ValueFactoryTest : public common_internal::ThreadCompatibleMemoryTest<> {
 public:
  void SetUp() override {
    value_manager_ = NewThreadCompatibleValueManager(
        memory_manager(), NewThreadCompatibleTypeReflector(memory_manager()));
  }

  void TearDown() override { Finish(); }

  void Finish() {
    value_manager_.reset();
    ThreadCompatibleMemoryTest::Finish();
  }

  TypeFactory& type_factory() const { return value_manager(); }

  TypeManager& type_manager() const { return value_manager(); }

  ValueFactory& value_factory() const { return value_manager(); }

  ValueManager& value_manager() const { return **value_manager_; }

  static std::string ToString(
      TestParamInfo<std::tuple<MemoryManagement>> param) {
    std::ostringstream out;
    out << std::get<0>(param.param);
    return out.str();
  }

 private:
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ValueFactoryTest, JsonValueNull) {
  auto value = value_factory().CreateValueFromJson(kJsonNull);
  EXPECT_TRUE(InstanceOf<NullValue>(value));
}

TEST_P(ValueFactoryTest, JsonValueBool) {
  auto value = value_factory().CreateValueFromJson(true);
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  EXPECT_TRUE(Cast<BoolValue>(value).NativeValue());
}

TEST_P(ValueFactoryTest, JsonValueNumber) {
  auto value = value_factory().CreateValueFromJson(1.0);
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  EXPECT_EQ(Cast<DoubleValue>(value).NativeValue(), 1.0);
}

TEST_P(ValueFactoryTest, JsonValueString) {
  auto value = value_factory().CreateValueFromJson(absl::Cord("foo"));
  ASSERT_TRUE(InstanceOf<StringValue>(value));
  EXPECT_EQ(Cast<StringValue>(value).NativeString(), "foo");
}

JsonObject NewJsonObjectForTesting(bool with_array = true,
                                   bool with_nested_object = true);

JsonArray NewJsonArrayForTesting(bool with_nested_array = true,
                                 bool with_object = true) {
  JsonArrayBuilder builder;
  builder.push_back(kJsonNull);
  builder.push_back(true);
  builder.push_back(1.0);
  builder.push_back(absl::Cord("foo"));
  if (with_nested_array) {
    builder.push_back(NewJsonArrayForTesting(false, false));
  }
  if (with_object) {
    builder.push_back(NewJsonObjectForTesting(false, false));
  }
  return std::move(builder).Build();
}

JsonObject NewJsonObjectForTesting(bool with_array, bool with_nested_object) {
  JsonObjectBuilder builder;
  builder.insert_or_assign(absl::Cord("a"), kJsonNull);
  builder.insert_or_assign(absl::Cord("b"), true);
  builder.insert_or_assign(absl::Cord("c"), 1.0);
  builder.insert_or_assign(absl::Cord("d"), absl::Cord("foo"));
  if (with_array) {
    builder.insert_or_assign(absl::Cord("e"),
                             NewJsonArrayForTesting(false, false));
  }
  if (with_nested_object) {
    builder.insert_or_assign(absl::Cord("f"),
                             NewJsonObjectForTesting(false, false));
  }
  return std::move(builder).Build();
}

TEST_P(ValueFactoryTest, JsonValueArray) {
  auto value = value_factory().CreateValueFromJson(NewJsonArrayForTesting());
  ASSERT_TRUE(InstanceOf<ListValue>(value));
  EXPECT_EQ(Type(value.GetRuntimeType()), cel::ListType());
  auto list_value = Cast<ListValue>(value);
  EXPECT_THAT(list_value.IsEmpty(), IsOkAndHolds(false));
  EXPECT_THAT(list_value.Size(), IsOkAndHolds(6));
  EXPECT_EQ(list_value.DebugString(),
            "[null, true, 1.0, \"foo\", [null, true, 1.0, \"foo\"], {\"a\": "
            "null, \"b\": true, \"c\": 1.0, \"d\": \"foo\"}]");
  ASSERT_OK_AND_ASSIGN(auto element, list_value.Get(value_manager(), 0));
  EXPECT_TRUE(InstanceOf<NullValue>(element));
}

TEST_P(ValueFactoryTest, JsonValueObject) {
  auto value = value_factory().CreateValueFromJson(NewJsonObjectForTesting());
  ASSERT_TRUE(InstanceOf<MapValue>(value));
  auto map_value = Cast<MapValue>(value);
  EXPECT_THAT(map_value.IsEmpty(), IsOkAndHolds(false));
  EXPECT_THAT(map_value.Size(), IsOkAndHolds(6));
  EXPECT_EQ(map_value.DebugString(),
            "{\"a\": null, \"b\": true, \"c\": 1.0, \"d\": \"foo\", \"e\": "
            "[null, true, 1.0, \"foo\"], \"f\": {\"a\": null, \"b\": true, "
            "\"c\": 1.0, \"d\": \"foo\"}}");
  ASSERT_OK_AND_ASSIGN(auto keys, map_value.ListKeys(value_manager()));
  EXPECT_THAT(keys.Size(), IsOkAndHolds(6));

  ASSERT_OK_AND_ASSIGN(auto keys_iterator,
                       map_value.NewIterator(value_manager()));
  std::vector<StringValue> string_keys;
  while (keys_iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto key, keys_iterator->Next(value_manager()));
    string_keys.push_back(StringValue(Cast<StringValue>(key)));
  }
  EXPECT_THAT(string_keys,
              UnorderedElementsAreArray({StringValue("a"), StringValue("b"),
                                         StringValue("c"), StringValue("d"),
                                         StringValue("e"), StringValue("f")}));
  ASSERT_OK_AND_ASSIGN(auto has,
                       map_value.Has(value_manager(), StringValue("a")));
  ASSERT_TRUE(InstanceOf<BoolValue>(has));
  EXPECT_TRUE(Cast<BoolValue>(has).NativeValue());
  ASSERT_OK_AND_ASSIGN(
      has, map_value.Has(value_manager(), StringValue(absl::Cord("a"))));
  ASSERT_TRUE(InstanceOf<BoolValue>(has));
  EXPECT_TRUE(Cast<BoolValue>(has).NativeValue());

  ASSERT_OK_AND_ASSIGN(auto get,
                       map_value.Get(value_manager(), StringValue("a")));
  ASSERT_TRUE(InstanceOf<NullValue>(get));
  ASSERT_OK_AND_ASSIGN(
      get, map_value.Get(value_manager(), StringValue(absl::Cord("a"))));
  ASSERT_TRUE(InstanceOf<NullValue>(get));
}

INSTANTIATE_TEST_SUITE_P(
    ValueFactoryTest, ValueFactoryTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ValueFactoryTest::ToString);

}  // namespace
}  // namespace cel
