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
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(IntType, Kind) {
  EXPECT_EQ(IntType().kind(), IntType::kKind);
  EXPECT_EQ(Type(IntType()).kind(), IntType::kKind);
}

TEST(IntType, Name) {
  EXPECT_EQ(IntType().name(), IntType::kName);
  EXPECT_EQ(Type(IntType()).name(), IntType::kName);
}

TEST(IntType, DebugString) {
  {
    std::ostringstream out;
    out << IntType();
    EXPECT_EQ(out.str(), IntType::kName);
  }
  {
    std::ostringstream out;
    out << Type(IntType());
    EXPECT_EQ(out.str(), IntType::kName);
  }
}

TEST(IntType, Hash) {
  EXPECT_EQ(absl::HashOf(IntType()), absl::HashOf(IntType()));
}

TEST(IntType, Equal) {
  EXPECT_EQ(IntType(), IntType());
  EXPECT_EQ(Type(IntType()), IntType());
  EXPECT_EQ(IntType(), Type(IntType()));
  EXPECT_EQ(Type(IntType()), Type(IntType()));
}

}  // namespace
}  // namespace cel
