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

#include "eval/public/cel_number.h"

#include <cstdint>
#include <limits>

#include "absl/types/optional.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using ::testing::Optional;


TEST(CelNumber, GetNumberFromCelValue) {
  EXPECT_THAT(GetNumberFromCelValue(CelValue::CreateDouble(1.1)),
              Optional(CelNumber::FromDouble(1.1)));
  EXPECT_THAT(GetNumberFromCelValue(CelValue::CreateInt64(1)),
              Optional(CelNumber::FromDouble(1.0)));
  EXPECT_THAT(GetNumberFromCelValue(CelValue::CreateUint64(1)),
              Optional(CelNumber::FromDouble(1.0)));

  EXPECT_EQ(GetNumberFromCelValue(CelValue::CreateDuration(absl::Seconds(1))),
            absl::nullopt);
}



}  // namespace
}  // namespace google::api::expr::runtime
