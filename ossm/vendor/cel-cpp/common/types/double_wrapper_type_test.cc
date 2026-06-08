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

TEST(DoubleWrapperType, Kind) {
  EXPECT_EQ(DoubleWrapperType().kind(), DoubleWrapperType::kKind);
  EXPECT_EQ(Type(DoubleWrapperType()).kind(), DoubleWrapperType::kKind);
}

TEST(DoubleWrapperType, Name) {
  EXPECT_EQ(DoubleWrapperType().name(), DoubleWrapperType::kName);
  EXPECT_EQ(Type(DoubleWrapperType()).name(), DoubleWrapperType::kName);
}

TEST(DoubleWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << DoubleWrapperType();
    EXPECT_EQ(out.str(), DoubleWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DoubleWrapperType());
    EXPECT_EQ(out.str(), DoubleWrapperType::kName);
  }
}

TEST(DoubleWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleWrapperType()),
            absl::HashOf(DoubleWrapperType()));
}

TEST(DoubleWrapperType, Equal) {
  EXPECT_EQ(DoubleWrapperType(), DoubleWrapperType());
  EXPECT_EQ(Type(DoubleWrapperType()), DoubleWrapperType());
  EXPECT_EQ(DoubleWrapperType(), Type(DoubleWrapperType()));
  EXPECT_EQ(Type(DoubleWrapperType()), Type(DoubleWrapperType()));
}

}  // namespace
}  // namespace cel
