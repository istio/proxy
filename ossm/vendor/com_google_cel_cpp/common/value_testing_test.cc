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

#include "common/value_testing.h"

#include <utility>

#include "gtest/gtest-spi.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel::test {
namespace {

using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

TEST(BoolValueIs, Match) { EXPECT_THAT(BoolValue(true), BoolValueIs(true)); }

TEST(BoolValueIs, NoMatch) {
  EXPECT_THAT(BoolValue(false), Not(BoolValueIs(true)));
  EXPECT_THAT(IntValue(2), Not(BoolValueIs(true)));
}

TEST(BoolValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(IntValue(42), BoolValueIs(true)); }(),
      "kind is bool and is equal to true");
}

TEST(IntValueIs, Match) { EXPECT_THAT(IntValue(42), IntValueIs(42)); }

TEST(IntValueIs, NoMatch) {
  EXPECT_THAT(IntValue(-42), Not(IntValueIs(42)));
  EXPECT_THAT(UintValue(2), Not(IntValueIs(42)));
}

TEST(IntValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(UintValue(42), IntValueIs(42)); }(),
      "kind is int and is equal to 42");
}

TEST(UintValueIs, Match) { EXPECT_THAT(UintValue(42), UintValueIs(42)); }

TEST(UintValueIs, NoMatch) {
  EXPECT_THAT(UintValue(41), Not(UintValueIs(42)));
  EXPECT_THAT(IntValue(2), Not(UintValueIs(42)));
}

TEST(UintValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(IntValue(42), UintValueIs(42)); }(),
      "kind is uint and is equal to 42");
}

TEST(DoubleValueIs, Match) {
  EXPECT_THAT(DoubleValue(1.2), DoubleValueIs(1.2));
}

TEST(DoubleValueIs, NoMatch) {
  EXPECT_THAT(DoubleValue(41), Not(DoubleValueIs(1.2)));
  EXPECT_THAT(IntValue(2), Not(DoubleValueIs(1.2)));
}

TEST(DoubleValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(IntValue(42), DoubleValueIs(1.2)); }(),
      "kind is double and is equal to 1.2");
}

TEST(DurationValueIs, Match) {
  EXPECT_THAT(DurationValue(absl::Minutes(2)),
              DurationValueIs(absl::Minutes(2)));
}

TEST(DurationValueIs, NoMatch) {
  EXPECT_THAT(DurationValue(absl::Minutes(5)),
              Not(DurationValueIs(absl::Minutes(2))));
  EXPECT_THAT(IntValue(2), Not(DurationValueIs(absl::Minutes(2))));
}

TEST(DurationValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(IntValue(42), DurationValueIs(absl::Minutes(2))); }(),
      "kind is duration and is equal to 2m");
}

TEST(TimestampValueIs, Match) {
  EXPECT_THAT(TimestampValue(absl::UnixEpoch() + absl::Minutes(2)),
              TimestampValueIs(absl::UnixEpoch() + absl::Minutes(2)));
}

TEST(TimestampValueIs, NoMatch) {
  EXPECT_THAT(TimestampValue(absl::UnixEpoch()),
              Not(TimestampValueIs(absl::UnixEpoch() + absl::Minutes(2))));
  EXPECT_THAT(IntValue(2),
              Not(TimestampValueIs(absl::UnixEpoch() + absl::Minutes(2))));
}

TEST(TimestampValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() {
        EXPECT_THAT(IntValue(42),
                    TimestampValueIs(absl::UnixEpoch() + absl::Minutes(2)));
      }(),
      "kind is timestamp and is equal to 19");
}

TEST(StringValueIs, Match) {
  EXPECT_THAT(StringValue("hello!"), StringValueIs("hello!"));
}

TEST(StringValueIs, NoMatch) {
  EXPECT_THAT(StringValue("hello!"), Not(StringValueIs("goodbye!")));
  EXPECT_THAT(IntValue(2), Not(StringValueIs("goodbye!")));
}

TEST(StringValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(IntValue(42), StringValueIs("hello!")); }(),
      "kind is string and is equal to \"hello!\"");
}

TEST(BytesValueIs, Match) {
  EXPECT_THAT(BytesValue("hello!"), BytesValueIs("hello!"));
}

TEST(BytesValueIs, NoMatch) {
  EXPECT_THAT(BytesValue("hello!"), Not(BytesValueIs("goodbye!")));
  EXPECT_THAT(IntValue(2), Not(BytesValueIs("goodbye!")));
}

TEST(BytesValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() { EXPECT_THAT(IntValue(42), BytesValueIs("hello!")); }(),
      "kind is bytes and is equal to \"hello!\"");
}

TEST(ErrorValueIs, Match) {
  EXPECT_THAT(ErrorValue(absl::InternalError("test")),
              ErrorValueIs(StatusIs(absl::StatusCode::kInternal, "test")));
}

TEST(ErrorValueIs, NoMatch) {
  EXPECT_THAT(ErrorValue(absl::UnknownError("test")),
              Not(ErrorValueIs(StatusIs(absl::StatusCode::kInternal, "test"))));
  EXPECT_THAT(IntValue(2), Not(ErrorValueIs(_)));
}

TEST(ErrorValueIs, NonMatchMessage) {
  EXPECT_NONFATAL_FAILURE(
      []() {
        EXPECT_THAT(IntValue(42), ErrorValueIs(StatusIs(
                                      absl::StatusCode::kInternal, "test")));
      }(),
      "kind is *error* and");
}

using ValueMatcherTest = common_internal::ValueTest<>;

TEST_F(ValueMatcherTest, OptionalValueIsMatch) {
  EXPECT_THAT(OptionalValue::Of(IntValue(42), arena()),
              OptionalValueIs(IntValueIs(42)));
}

TEST_F(ValueMatcherTest, OptionalValueIsHeldValueDifferent) {
  EXPECT_NONFATAL_FAILURE(
      [&]() {
        EXPECT_THAT(OptionalValue::Of(IntValue(-42), arena()),
                    OptionalValueIs(IntValueIs(42)));
      }(),
      "is OptionalValue that is engaged with value whose kind is int and is "
      "equal to 42");
}

TEST_F(ValueMatcherTest, OptionalValueIsNotEngaged) {
  EXPECT_NONFATAL_FAILURE(
      [&]() {
        EXPECT_THAT(OptionalValue::None(), OptionalValueIs(IntValueIs(42)));
      }(),
      "is not engaged");
}

TEST_F(ValueMatcherTest, OptionalValueIsNotAnOptional) {
  EXPECT_NONFATAL_FAILURE(
      [&]() { EXPECT_THAT(IntValue(42), OptionalValueIs(IntValueIs(42))); }(),
      "wanted OptionalValue, got int");
}

TEST_F(ValueMatcherTest, OptionalValueIsEmptyMatch) {
  EXPECT_THAT(OptionalValue::None(), OptionalValueIsEmpty());
}

TEST_F(ValueMatcherTest, OptionalValueIsEmptyNotEmpty) {
  EXPECT_NONFATAL_FAILURE(
      [&]() {
        EXPECT_THAT(OptionalValue::Of(IntValue(42), arena()),
                    OptionalValueIsEmpty());
      }(),
      "is not empty");
}

TEST_F(ValueMatcherTest, OptionalValueIsEmptyNotOptional) {
  EXPECT_NONFATAL_FAILURE(
      [&]() { EXPECT_THAT(IntValue(42), OptionalValueIsEmpty()); }(),
      "wanted OptionalValue, got int");
}

TEST_F(ValueMatcherTest, ListMatcherBasic) {
  auto builder = NewListValueBuilder(arena());

  ASSERT_OK(builder->Add(IntValue(42)));

  Value list_value = std::move(*builder).Build();

  EXPECT_THAT(list_value, ListValueIs(Truly([](const ListValue& v) {
                auto size = v.Size();
                return size.ok() && *size == 1;
              })));
}

TEST_F(ValueMatcherTest, ListMatcherMatchesElements) {
  auto builder = NewListValueBuilder(arena());
  ASSERT_OK(builder->Add(IntValue(42)));
  ASSERT_OK(builder->Add(IntValue(1337)));
  ASSERT_OK(builder->Add(IntValue(42)));
  ASSERT_OK(builder->Add(IntValue(100)));
  EXPECT_THAT(std::move(*builder).Build(),
              ListValueIs(ListValueElements(
                  ElementsAre(IntValueIs(42), IntValueIs(1337), IntValueIs(42),
                              IntValueIs(100)),
                  descriptor_pool(), message_factory(), arena())));
}

TEST_F(ValueMatcherTest, MapMatcherBasic) {
  auto builder = NewMapValueBuilder(arena());

  ASSERT_OK(builder->Put(IntValue(42), IntValue(42)));

  Value map_value = std::move(*builder).Build();

  EXPECT_THAT(map_value, MapValueIs(Truly([](const MapValue& v) {
                auto size = v.Size();
                return size.ok() && *size == 1;
              })));
}

TEST_F(ValueMatcherTest, MapMatcherMatchesElements) {
  auto builder = NewMapValueBuilder(arena());

  ASSERT_OK(builder->Put(IntValue(42), StringValue("answer")));
  ASSERT_OK(builder->Put(IntValue(1337), StringValue("leet")));
  EXPECT_THAT(
      std::move(*builder).Build(),
      MapValueIs(MapValueElements(
          UnorderedElementsAre(Pair(IntValueIs(42), StringValueIs("answer")),
                               Pair(IntValueIs(1337), StringValueIs("leet"))),
          descriptor_pool(), message_factory(), arena())));
}

}  // namespace
}  // namespace cel::test
