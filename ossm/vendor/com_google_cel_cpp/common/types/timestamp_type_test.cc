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

TEST(TimestampType, Kind) {
  EXPECT_EQ(TimestampType().kind(), TimestampType::kKind);
  EXPECT_EQ(Type(TimestampType()).kind(), TimestampType::kKind);
}

TEST(TimestampType, Name) {
  EXPECT_EQ(TimestampType().name(), TimestampType::kName);
  EXPECT_EQ(Type(TimestampType()).name(), TimestampType::kName);
}

TEST(TimestampType, DebugString) {
  {
    std::ostringstream out;
    out << TimestampType();
    EXPECT_EQ(out.str(), TimestampType::kName);
  }
  {
    std::ostringstream out;
    out << Type(TimestampType());
    EXPECT_EQ(out.str(), TimestampType::kName);
  }
}

TEST(TimestampType, Hash) {
  EXPECT_EQ(absl::HashOf(TimestampType()), absl::HashOf(TimestampType()));
}

TEST(TimestampType, Equal) {
  EXPECT_EQ(TimestampType(), TimestampType());
  EXPECT_EQ(Type(TimestampType()), TimestampType());
  EXPECT_EQ(TimestampType(), Type(TimestampType()));
  EXPECT_EQ(Type(TimestampType()), Type(TimestampType()));
}

}  // namespace
}  // namespace cel
