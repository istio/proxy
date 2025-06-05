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

TEST(IntWrapperType, Kind) {
  EXPECT_EQ(IntWrapperType().kind(), IntWrapperType::kKind);
  EXPECT_EQ(Type(IntWrapperType()).kind(), IntWrapperType::kKind);
}

TEST(IntWrapperType, Name) {
  EXPECT_EQ(IntWrapperType().name(), IntWrapperType::kName);
  EXPECT_EQ(Type(IntWrapperType()).name(), IntWrapperType::kName);
}

TEST(IntWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << IntWrapperType();
    EXPECT_EQ(out.str(), IntWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(IntWrapperType());
    EXPECT_EQ(out.str(), IntWrapperType::kName);
  }
}

TEST(IntWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(IntWrapperType()), absl::HashOf(IntWrapperType()));
}

TEST(IntWrapperType, Equal) {
  EXPECT_EQ(IntWrapperType(), IntWrapperType());
  EXPECT_EQ(Type(IntWrapperType()), IntWrapperType());
  EXPECT_EQ(IntWrapperType(), Type(IntWrapperType()));
  EXPECT_EQ(Type(IntWrapperType()), Type(IntWrapperType()));
}

}  // namespace
}  // namespace cel
