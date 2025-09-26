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

#include <cmath>
#include <sstream>

#include "absl/status/status_matchers.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;

using DoubleValueTest = common_internal::ValueTest<>;

TEST_F(DoubleValueTest, Kind) {
  EXPECT_EQ(DoubleValue(1.0).kind(), DoubleValue::kKind);
  EXPECT_EQ(Value(DoubleValue(1.0)).kind(), DoubleValue::kKind);
}

TEST_F(DoubleValueTest, DebugString) {
  {
    std::ostringstream out;
    out << DoubleValue(0.0);
    EXPECT_EQ(out.str(), "0.0");
  }
  {
    std::ostringstream out;
    out << DoubleValue(1.0);
    EXPECT_EQ(out.str(), "1.0");
  }
  {
    std::ostringstream out;
    out << DoubleValue(1.1);
    EXPECT_EQ(out.str(), "1.1");
  }
  {
    std::ostringstream out;
    out << DoubleValue(NAN);
    EXPECT_EQ(out.str(), "nan");
  }
  {
    std::ostringstream out;
    out << DoubleValue(INFINITY);
    EXPECT_EQ(out.str(), "+infinity");
  }
  {
    std::ostringstream out;
    out << DoubleValue(-INFINITY);
    EXPECT_EQ(out.str(), "-infinity");
  }
  {
    std::ostringstream out;
    out << Value(DoubleValue(0.0));
    EXPECT_EQ(out.str(), "0.0");
  }
}

TEST_F(DoubleValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(DoubleValue(1.0).ConvertToJson(descriptor_pool(),
                                             message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(number_value: 1)pb"));
}

TEST_F(DoubleValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleValue(1.0)),
            NativeTypeId::For<DoubleValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(DoubleValue(1.0))),
            NativeTypeId::For<DoubleValue>());
}

TEST_F(DoubleValueTest, Equality) {
  EXPECT_NE(DoubleValue(0.0), 1.0);
  EXPECT_NE(1.0, DoubleValue(0.0));
  EXPECT_NE(DoubleValue(0.0), DoubleValue(1.0));
}

}  // namespace
}  // namespace cel
