// Copyright 2022 Google LLC
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

#include "internal/number.h"

#include <cstdint>
#include <limits>

#include "internal/testing.h"

namespace cel::internal {
namespace {

TEST(Number, Basic) {
  EXPECT_GT(Number(1.1), Number::FromInt64(1));
  EXPECT_LT(Number::FromUint64(1), Number(1.1));
  EXPECT_EQ(Number(1.1), Number(1.1));

  EXPECT_EQ(Number::FromUint64(1), Number::FromUint64(1));
  EXPECT_EQ(Number::FromInt64(1), Number::FromUint64(1));
  EXPECT_GT(Number::FromUint64(1), Number::FromInt64(-1));

  EXPECT_EQ(Number::FromInt64(-1), Number::FromInt64(-1));
}

TEST(Number, Conversions) {
  EXPECT_TRUE(Number::FromDouble(1.0).LosslessConvertibleToInt());
  EXPECT_TRUE(Number::FromDouble(1.0).LosslessConvertibleToUint());
  EXPECT_FALSE(Number::FromDouble(1.1).LosslessConvertibleToInt());
  EXPECT_FALSE(Number::FromDouble(1.1).LosslessConvertibleToUint());
  EXPECT_TRUE(Number::FromDouble(-1.0).LosslessConvertibleToInt());
  EXPECT_FALSE(Number::FromDouble(-1.0).LosslessConvertibleToUint());
  EXPECT_TRUE(Number::FromDouble(kDoubleToIntMin).LosslessConvertibleToInt());

  // Need to add/substract a large number since double resolution is low at this
  // range.
  EXPECT_FALSE(Number::FromDouble(kMaxDoubleRepresentableAsUint +
                                  RoundingError<uint64_t>())
                   .LosslessConvertibleToUint());
  EXPECT_FALSE(Number::FromDouble(kMaxDoubleRepresentableAsInt +
                                  RoundingError<int64_t>())
                   .LosslessConvertibleToInt());
  EXPECT_FALSE(
      Number::FromDouble(kDoubleToIntMin - 1025).LosslessConvertibleToInt());

  EXPECT_EQ(Number::FromInt64(1).AsUint(), 1u);
  EXPECT_EQ(Number::FromUint64(1).AsInt(), 1);
  EXPECT_EQ(Number::FromDouble(1.0).AsUint(), 1);
  EXPECT_EQ(Number::FromDouble(1.0).AsInt(), 1);
}

}  // namespace
}  // namespace cel::internal
