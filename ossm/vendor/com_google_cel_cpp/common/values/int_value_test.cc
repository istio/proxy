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

using IntValueTest = common_internal::ValueTest<>;

TEST_F(IntValueTest, Kind) {
  EXPECT_EQ(IntValue(1).kind(), IntValue::kKind);
  EXPECT_EQ(Value(IntValue(1)).kind(), IntValue::kKind);
}

TEST_F(IntValueTest, DebugString) {
  {
    std::ostringstream out;
    out << IntValue(1);
    EXPECT_EQ(out.str(), "1");
  }
  {
    std::ostringstream out;
    out << Value(IntValue(1));
    EXPECT_EQ(out.str(), "1");
  }
}

TEST_F(IntValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(
      IntValue(1).ConvertToJson(descriptor_pool(), message_factory(), message),
      IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(number_value: 1)pb"));
}

TEST_F(IntValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntValue(1)), NativeTypeId::For<IntValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(IntValue(1))),
            NativeTypeId::For<IntValue>());
}

TEST_F(IntValueTest, HashValue) {
  EXPECT_EQ(absl::HashOf(IntValue(1)), absl::HashOf(int64_t{1}));
}

TEST_F(IntValueTest, Equality) {
  EXPECT_NE(IntValue(0), 1);
  EXPECT_NE(1, IntValue(0));
  EXPECT_NE(IntValue(0), IntValue(1));
}

TEST_F(IntValueTest, LessThan) {
  EXPECT_LT(IntValue(0), 1);
  EXPECT_LT(0, IntValue(1));
  EXPECT_LT(IntValue(0), IntValue(1));
}

}  // namespace
}  // namespace cel
