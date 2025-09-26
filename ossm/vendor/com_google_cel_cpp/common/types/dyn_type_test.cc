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

TEST(DynType, Kind) {
  EXPECT_EQ(DynType().kind(), DynType::kKind);
  EXPECT_EQ(Type(DynType()).kind(), DynType::kKind);
}

TEST(DynType, Name) {
  EXPECT_EQ(DynType().name(), DynType::kName);
  EXPECT_EQ(Type(DynType()).name(), DynType::kName);
}

TEST(DynType, DebugString) {
  {
    std::ostringstream out;
    out << DynType();
    EXPECT_EQ(out.str(), DynType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DynType());
    EXPECT_EQ(out.str(), DynType::kName);
  }
}

TEST(DynType, Hash) {
  EXPECT_EQ(absl::HashOf(DynType()), absl::HashOf(DynType()));
}

TEST(DynType, Equal) {
  EXPECT_EQ(DynType(), DynType());
  EXPECT_EQ(Type(DynType()), DynType());
  EXPECT_EQ(DynType(), Type(DynType()));
  EXPECT_EQ(Type(DynType()), Type(DynType()));
}

}  // namespace
}  // namespace cel
