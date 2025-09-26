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
#include <sstream>

#include "absl/hash/hash.h"
#include "absl/status/status_matchers.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;

using UintValueTest = common_internal::ValueTest<>;

TEST_F(UintValueTest, Kind) {
  EXPECT_EQ(UintValue(1).kind(), UintValue::kKind);
  EXPECT_EQ(Value(UintValue(1)).kind(), UintValue::kKind);
}

TEST_F(UintValueTest, DebugString) {
  {
    std::ostringstream out;
    out << UintValue(1);
    EXPECT_EQ(out.str(), "1u");
  }
  {
    std::ostringstream out;
    out << Value(UintValue(1));
    EXPECT_EQ(out.str(), "1u");
  }
}

TEST_F(UintValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(
      UintValue(1).ConvertToJson(descriptor_pool(), message_factory(), message),
      IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(number_value: 1)pb"));
}

TEST_F(UintValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintValue(1)), NativeTypeId::For<UintValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(UintValue(1))),
            NativeTypeId::For<UintValue>());
}

TEST_F(UintValueTest, HashValue) {
  EXPECT_EQ(absl::HashOf(UintValue(1)), absl::HashOf(uint64_t{1}));
}

TEST_F(UintValueTest, Equality) {
  EXPECT_NE(UintValue(0u), 1u);
  EXPECT_NE(1u, UintValue(0u));
  EXPECT_NE(UintValue(0u), UintValue(1u));
}

TEST_F(UintValueTest, LessThan) {
  EXPECT_LT(UintValue(0), 1);
  EXPECT_LT(0, UintValue(1));
  EXPECT_LT(UintValue(0), UintValue(1));
}

}  // namespace
}  // namespace cel
