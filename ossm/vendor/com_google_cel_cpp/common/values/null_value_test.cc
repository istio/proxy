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

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::testing::An;
using ::testing::Ne;

using NullValueTest = common_internal::ValueTest<>;

TEST_F(NullValueTest, Kind) {
  EXPECT_EQ(NullValue().kind(), NullValue::kKind);
  EXPECT_EQ(Value(NullValue()).kind(), NullValue::kKind);
}

TEST_F(NullValueTest, DebugString) {
  {
    std::ostringstream out;
    out << NullValue();
    EXPECT_EQ(out.str(), "null");
  }
  {
    std::ostringstream out;
    out << Value(NullValue());
    EXPECT_EQ(out.str(), "null");
  }
}

TEST_F(NullValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(
      NullValue().ConvertToJson(descriptor_pool(), message_factory(), message),
      IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(null_value: NULL_VALUE)pb"));
}

TEST_F(NullValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(NullValue()), NativeTypeId::For<NullValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(NullValue())),
            NativeTypeId::For<NullValue>());
}

TEST_F(NullValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<NullValue>(NullValue()));
  EXPECT_TRUE(InstanceOf<NullValue>(Value(NullValue())));
}

TEST_F(NullValueTest, Cast) {
  EXPECT_THAT(Cast<NullValue>(NullValue()), An<NullValue>());
  EXPECT_THAT(Cast<NullValue>(Value(NullValue())), An<NullValue>());
}

TEST_F(NullValueTest, As) {
  EXPECT_THAT(As<NullValue>(Value(NullValue())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
