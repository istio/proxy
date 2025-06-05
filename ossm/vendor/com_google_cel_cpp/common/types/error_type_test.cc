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

TEST(ErrorType, Kind) {
  EXPECT_EQ(ErrorType().kind(), ErrorType::kKind);
  EXPECT_EQ(Type(ErrorType()).kind(), ErrorType::kKind);
}

TEST(ErrorType, Name) {
  EXPECT_EQ(ErrorType().name(), ErrorType::kName);
  EXPECT_EQ(Type(ErrorType()).name(), ErrorType::kName);
}

TEST(ErrorType, DebugString) {
  {
    std::ostringstream out;
    out << ErrorType();
    EXPECT_EQ(out.str(), ErrorType::kName);
  }
  {
    std::ostringstream out;
    out << Type(ErrorType());
    EXPECT_EQ(out.str(), ErrorType::kName);
  }
}

TEST(ErrorType, Hash) {
  EXPECT_EQ(absl::HashOf(ErrorType()), absl::HashOf(ErrorType()));
}

TEST(ErrorType, Equal) {
  EXPECT_EQ(ErrorType(), ErrorType());
  EXPECT_EQ(Type(ErrorType()), ErrorType());
  EXPECT_EQ(ErrorType(), Type(ErrorType()));
  EXPECT_EQ(Type(ErrorType()), Type(ErrorType()));
}

}  // namespace
}  // namespace cel
