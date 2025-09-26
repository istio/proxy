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

TEST(DoubleType, Kind) {
  EXPECT_EQ(DoubleType().kind(), DoubleType::kKind);
  EXPECT_EQ(Type(DoubleType()).kind(), DoubleType::kKind);
}

TEST(DoubleType, Name) {
  EXPECT_EQ(DoubleType().name(), DoubleType::kName);
  EXPECT_EQ(Type(DoubleType()).name(), DoubleType::kName);
}

TEST(DoubleType, DebugString) {
  {
    std::ostringstream out;
    out << DoubleType();
    EXPECT_EQ(out.str(), DoubleType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DoubleType());
    EXPECT_EQ(out.str(), DoubleType::kName);
  }
}

TEST(DoubleType, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleType()), absl::HashOf(DoubleType()));
}

TEST(DoubleType, Equal) {
  EXPECT_EQ(DoubleType(), DoubleType());
  EXPECT_EQ(Type(DoubleType()), DoubleType());
  EXPECT_EQ(DoubleType(), Type(DoubleType()));
  EXPECT_EQ(Type(DoubleType()), Type(DoubleType()));
}

}  // namespace
}  // namespace cel
