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

TEST(NullType, Kind) {
  EXPECT_EQ(NullType().kind(), NullType::kKind);
  EXPECT_EQ(Type(NullType()).kind(), NullType::kKind);
}

TEST(NullType, Name) {
  EXPECT_EQ(NullType().name(), NullType::kName);
  EXPECT_EQ(Type(NullType()).name(), NullType::kName);
}

TEST(NullType, DebugString) {
  {
    std::ostringstream out;
    out << NullType();
    EXPECT_EQ(out.str(), NullType::kName);
  }
  {
    std::ostringstream out;
    out << Type(NullType());
    EXPECT_EQ(out.str(), NullType::kName);
  }
}

TEST(NullType, Hash) {
  EXPECT_EQ(absl::HashOf(NullType()), absl::HashOf(NullType()));
}

TEST(NullType, Equal) {
  EXPECT_EQ(NullType(), NullType());
  EXPECT_EQ(Type(NullType()), NullType());
  EXPECT_EQ(NullType(), Type(NullType()));
  EXPECT_EQ(Type(NullType()), Type(NullType()));
}

}  // namespace
}  // namespace cel
