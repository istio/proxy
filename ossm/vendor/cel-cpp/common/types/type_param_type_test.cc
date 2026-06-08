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

#include "common/type.h"

#include <sstream>

#include "absl/hash/hash.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(TypeParamType, Kind) {
  EXPECT_EQ(TypeParamType("T").kind(), TypeParamType::kKind);
  EXPECT_EQ(Type(TypeParamType("T")).kind(), TypeParamType::kKind);
}

TEST(TypeParamType, Name) {
  EXPECT_EQ(TypeParamType("T").name(), "T");
  EXPECT_EQ(Type(TypeParamType("T")).name(), "T");
}

TEST(TypeParamType, DebugString) {
  {
    std::ostringstream out;
    out << TypeParamType("T");
    EXPECT_EQ(out.str(), "T");
  }
  {
    std::ostringstream out;
    out << Type(TypeParamType("T"));
    EXPECT_EQ(out.str(), "T");
  }
}

TEST(TypeParamType, Hash) {
  EXPECT_EQ(absl::HashOf(TypeParamType("T")), absl::HashOf(TypeParamType("T")));
}

TEST(TypeParamType, Equal) {
  EXPECT_EQ(TypeParamType("T"), TypeParamType("T"));
  EXPECT_EQ(Type(TypeParamType("T")), TypeParamType("T"));
  EXPECT_EQ(TypeParamType("T"), Type(TypeParamType("T")));
  EXPECT_EQ(Type(TypeParamType("T")), Type(TypeParamType("T")));
}

}  // namespace
}  // namespace cel
