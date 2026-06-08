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

#include "common/type.h"

#include <sstream>

#include "absl/hash/hash.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(TypeType, Kind) {
  EXPECT_EQ(TypeType().kind(), TypeType::kKind);
  EXPECT_EQ(Type(TypeType()).kind(), TypeType::kKind);
}

TEST(TypeType, Name) {
  EXPECT_EQ(TypeType().name(), TypeType::kName);
  EXPECT_EQ(Type(TypeType()).name(), TypeType::kName);
}

TEST(TypeType, DebugString) {
  {
    std::ostringstream out;
    out << TypeType();
    EXPECT_EQ(out.str(), TypeType::kName);
  }
  {
    std::ostringstream out;
    out << Type(TypeType());
    EXPECT_EQ(out.str(), TypeType::kName);
  }
}

TEST(TypeType, Hash) {
  EXPECT_EQ(absl::HashOf(TypeType()), absl::HashOf(TypeType()));
}

TEST(TypeType, Equal) {
  EXPECT_EQ(TypeType(), TypeType());
  EXPECT_EQ(Type(TypeType()), TypeType());
  EXPECT_EQ(TypeType(), Type(TypeType()));
  EXPECT_EQ(Type(TypeType()), Type(TypeType()));
}

}  // namespace
}  // namespace cel
