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

TEST(UnknownType, Kind) {
  EXPECT_EQ(UnknownType().kind(), UnknownType::kKind);
  EXPECT_EQ(Type(UnknownType()).kind(), UnknownType::kKind);
}

TEST(UnknownType, Name) {
  EXPECT_EQ(UnknownType().name(), UnknownType::kName);
  EXPECT_EQ(Type(UnknownType()).name(), UnknownType::kName);
}

TEST(UnknownType, DebugString) {
  {
    std::ostringstream out;
    out << UnknownType();
    EXPECT_EQ(out.str(), UnknownType::kName);
  }
  {
    std::ostringstream out;
    out << Type(UnknownType());
    EXPECT_EQ(out.str(), UnknownType::kName);
  }
}

TEST(UnknownType, Hash) {
  EXPECT_EQ(absl::HashOf(UnknownType()), absl::HashOf(UnknownType()));
}

TEST(UnknownType, Equal) {
  EXPECT_EQ(UnknownType(), UnknownType());
  EXPECT_EQ(Type(UnknownType()), UnknownType());
  EXPECT_EQ(UnknownType(), Type(UnknownType()));
  EXPECT_EQ(Type(UnknownType()), Type(UnknownType()));
}

}  // namespace
}  // namespace cel
