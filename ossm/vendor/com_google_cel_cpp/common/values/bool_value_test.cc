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

using BoolValueTest = common_internal::ValueTest<>;

TEST_F(BoolValueTest, Kind) {
  EXPECT_EQ(BoolValue(true).kind(), BoolValue::kKind);
  EXPECT_EQ(Value(BoolValue(true)).kind(), BoolValue::kKind);
}

TEST_F(BoolValueTest, DebugString) {
  {
    std::ostringstream out;
    out << BoolValue(true);
    EXPECT_EQ(out.str(), "true");
  }
  {
    std::ostringstream out;
    out << Value(BoolValue(true));
    EXPECT_EQ(out.str(), "true");
  }
}

TEST_F(BoolValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(BoolValue(false).ConvertToJson(descriptor_pool(),
                                             message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(bool_value: false)pb"));
}

TEST_F(BoolValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BoolValue(true)), NativeTypeId::For<BoolValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BoolValue(true))),
            NativeTypeId::For<BoolValue>());
}

TEST_F(BoolValueTest, HashValue) {
  EXPECT_EQ(absl::HashOf(BoolValue(true)), absl::HashOf(true));
}

TEST_F(BoolValueTest, Equality) {
  EXPECT_NE(BoolValue(false), true);
  EXPECT_NE(true, BoolValue(false));
  EXPECT_NE(BoolValue(false), BoolValue(true));
}

TEST_F(BoolValueTest, LessThan) {
  EXPECT_LT(BoolValue(false), true);
  EXPECT_LT(false, BoolValue(true));
  EXPECT_LT(BoolValue(false), BoolValue(true));
}

}  // namespace
}  // namespace cel
