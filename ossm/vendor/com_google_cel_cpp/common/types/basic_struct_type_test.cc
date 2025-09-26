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

#include "common/type.h"
#include "common/type_kind.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;

TEST(BasicStructType, Kind) {
  EXPECT_EQ(BasicStructType::kind(), TypeKind::kStruct);
}

TEST(BasicStructType, Default) {
  BasicStructType type;
  EXPECT_FALSE(type);
  EXPECT_THAT(type.DebugString(), Eq(""));
  EXPECT_EQ(type, BasicStructType());
}

TEST(BasicStructType, Name) {
  BasicStructType type = MakeBasicStructType("test.Struct");
  EXPECT_TRUE(type);
  EXPECT_THAT(type.name(), Eq("test.Struct"));
  EXPECT_THAT(type.DebugString(), Eq("test.Struct"));
  EXPECT_THAT(type.GetParameters(), IsEmpty());
  EXPECT_NE(type, BasicStructType());
  EXPECT_NE(BasicStructType(), type);
}

}  // namespace
}  // namespace cel::common_internal
