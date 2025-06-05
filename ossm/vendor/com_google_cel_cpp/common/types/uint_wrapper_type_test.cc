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

TEST(UintWrapperType, Kind) {
  EXPECT_EQ(UintWrapperType().kind(), UintWrapperType::kKind);
  EXPECT_EQ(Type(UintWrapperType()).kind(), UintWrapperType::kKind);
}

TEST(UintWrapperType, Name) {
  EXPECT_EQ(UintWrapperType().name(), UintWrapperType::kName);
  EXPECT_EQ(Type(UintWrapperType()).name(), UintWrapperType::kName);
}

TEST(UintWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << UintWrapperType();
    EXPECT_EQ(out.str(), UintWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(UintWrapperType());
    EXPECT_EQ(out.str(), UintWrapperType::kName);
  }
}

TEST(UintWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(UintWrapperType()), absl::HashOf(UintWrapperType()));
}

TEST(UintWrapperType, Equal) {
  EXPECT_EQ(UintWrapperType(), UintWrapperType());
  EXPECT_EQ(Type(UintWrapperType()), UintWrapperType());
  EXPECT_EQ(UintWrapperType(), Type(UintWrapperType()));
  EXPECT_EQ(Type(UintWrapperType()), Type(UintWrapperType()));
}

}  // namespace
}  // namespace cel
