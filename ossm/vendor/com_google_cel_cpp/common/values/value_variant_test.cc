// Copyright 2025 Google LLC
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

#include "absl/strings/cord.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

template <typename T>
class ValueVariantTest : public ::testing::Test {};

#define VALUE_VARIANT_TYPES(T)                                               \
  std::pair<NullValue, T>, std::pair<BoolValue, T>, std::pair<IntValue, T>,  \
      std::pair<UintValue, T>, std::pair<DoubleValue, T>,                    \
      std::pair<DurationValue, T>, std::pair<TimestampValue, T>,             \
      std::pair<TypeValue, T>, std::pair<LegacyListValue, T>,                \
      std::pair<ParsedJsonListValue, T>,                                     \
      std::pair<ParsedRepeatedFieldValue, T>, std::pair<CustomListValue, T>, \
      std::pair<LegacyMapValue, T>, std::pair<ParsedJsonMapValue, T>,        \
      std::pair<ParsedMapFieldValue, T>, std::pair<CustomMapValue, T>,       \
      std::pair<LegacyStructValue, T>, std::pair<ParsedMessageValue, T>,     \
      std::pair<CustomStructValue, T>, std::pair<OpaqueValue, T>,            \
      std::pair<BytesValue, T>, std::pair<StringValue, T>,                   \
      std::pair<ErrorValue, T>, std::pair<UnknownValue, T>

using ValueVariantTypes = ::testing::Types<
    VALUE_VARIANT_TYPES(NullValue), VALUE_VARIANT_TYPES(BoolValue),
    VALUE_VARIANT_TYPES(IntValue), VALUE_VARIANT_TYPES(UintValue),
    VALUE_VARIANT_TYPES(DoubleValue), VALUE_VARIANT_TYPES(DurationValue),
    VALUE_VARIANT_TYPES(TimestampValue), VALUE_VARIANT_TYPES(TypeValue),
    VALUE_VARIANT_TYPES(LegacyListValue),
    VALUE_VARIANT_TYPES(ParsedJsonListValue),
    VALUE_VARIANT_TYPES(ParsedRepeatedFieldValue),
    VALUE_VARIANT_TYPES(CustomListValue), VALUE_VARIANT_TYPES(LegacyMapValue),
    VALUE_VARIANT_TYPES(ParsedJsonMapValue),
    VALUE_VARIANT_TYPES(ParsedMapFieldValue),
    VALUE_VARIANT_TYPES(CustomMapValue), VALUE_VARIANT_TYPES(LegacyStructValue),
    VALUE_VARIANT_TYPES(ParsedMessageValue),
    VALUE_VARIANT_TYPES(CustomStructValue), VALUE_VARIANT_TYPES(OpaqueValue),
    VALUE_VARIANT_TYPES(BytesValue), VALUE_VARIANT_TYPES(StringValue),
    VALUE_VARIANT_TYPES(ErrorValue), VALUE_VARIANT_TYPES(UnknownValue)>;

template <typename T>
struct DefaultValue {
  T operator()() const { return T(); }
};

template <>
struct DefaultValue<BytesValue> {
  BytesValue operator()() const {
    return BytesValue(
        absl::Cord("Some somewhat large string that is not storable inline!"));
  }
};

template <>
struct DefaultValue<StringValue> {
  StringValue operator()() const {
    return StringValue(
        absl::Cord("Some somewhat large string that is not storable inline!"));
  }
};

#undef VALUE_VARIANT_TYPES

TYPED_TEST_SUITE(ValueVariantTest, ValueVariantTypes);

TYPED_TEST(ValueVariantTest, CopyAssign) {
  using Left = typename TypeParam::first_type;
  using Right = typename TypeParam::second_type;

  ValueVariant lhs(DefaultValue<Left>{}());
  ValueVariant rhs(DefaultValue<Right>{}());

  EXPECT_TRUE(lhs.Is<Left>());

  lhs = rhs;

  EXPECT_TRUE(lhs.Is<Right>());
  EXPECT_TRUE(rhs.Is<Right>());
}

TYPED_TEST(ValueVariantTest, MoveAssign) {
  using Left = typename TypeParam::first_type;
  using Right = typename TypeParam::second_type;

  ValueVariant lhs(DefaultValue<Left>{}());
  ValueVariant rhs(DefaultValue<Right>{}());

  EXPECT_TRUE(lhs.Is<Left>());

  lhs = std::move(rhs);

  EXPECT_TRUE(lhs.Is<Right>());
}

TYPED_TEST(ValueVariantTest, Swap) {
  using Left = typename TypeParam::first_type;
  using Right = typename TypeParam::second_type;

  ValueVariant lhs(DefaultValue<Left>{}());
  ValueVariant rhs(DefaultValue<Right>{}());

  swap(lhs, rhs);

  EXPECT_TRUE(lhs.Is<Right>());
  EXPECT_TRUE(rhs.Is<Left>());
}

}  // namespace
}  // namespace cel::common_internal
