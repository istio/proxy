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

TEST(DurationType, Kind) {
  EXPECT_EQ(DurationType().kind(), DurationType::kKind);
  EXPECT_EQ(Type(DurationType()).kind(), DurationType::kKind);
}

TEST(DurationType, Name) {
  EXPECT_EQ(DurationType().name(), DurationType::kName);
  EXPECT_EQ(Type(DurationType()).name(), DurationType::kName);
}

TEST(DurationType, DebugString) {
  {
    std::ostringstream out;
    out << DurationType();
    EXPECT_EQ(out.str(), DurationType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DurationType());
    EXPECT_EQ(out.str(), DurationType::kName);
  }
}

TEST(DurationType, Hash) {
  EXPECT_EQ(absl::HashOf(DurationType()), absl::HashOf(DurationType()));
}

TEST(DurationType, Equal) {
  EXPECT_EQ(DurationType(), DurationType());
  EXPECT_EQ(Type(DurationType()), DurationType());
  EXPECT_EQ(DurationType(), Type(DurationType()));
  EXPECT_EQ(Type(DurationType()), Type(DurationType()));
}

}  // namespace
}  // namespace cel
