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

TEST(UintType, Kind) {
  EXPECT_EQ(UintType().kind(), UintType::kKind);
  EXPECT_EQ(Type(UintType()).kind(), UintType::kKind);
}

TEST(UintType, Name) {
  EXPECT_EQ(UintType().name(), UintType::kName);
  EXPECT_EQ(Type(UintType()).name(), UintType::kName);
}

TEST(UintType, DebugString) {
  {
    std::ostringstream out;
    out << UintType();
    EXPECT_EQ(out.str(), UintType::kName);
  }
  {
    std::ostringstream out;
    out << Type(UintType());
    EXPECT_EQ(out.str(), UintType::kName);
  }
}

TEST(UintType, Hash) {
  EXPECT_EQ(absl::HashOf(UintType()), absl::HashOf(UintType()));
}

TEST(UintType, Equal) {
  EXPECT_EQ(UintType(), UintType());
  EXPECT_EQ(Type(UintType()), UintType());
  EXPECT_EQ(UintType(), Type(UintType()));
  EXPECT_EQ(Type(UintType()), Type(UintType()));
}

}  // namespace
}  // namespace cel
