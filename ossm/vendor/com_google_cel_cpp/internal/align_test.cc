// Copyright 2024 Google LLC
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

#include "internal/align.h"

#include <cstddef>
#include <cstdint>

#include "internal/testing.h"

namespace cel::internal {
namespace {

TEST(AlignmentMask, Masks) {
  EXPECT_EQ(AlignmentMask(size_t{1}), size_t{0});
  EXPECT_EQ(AlignmentMask(size_t{2}), size_t{1});
  EXPECT_EQ(AlignmentMask(size_t{4}), size_t{3});
}

TEST(AlignDown, Aligns) {
  EXPECT_EQ(AlignDown(uintptr_t{3}, 4), 0);
  EXPECT_EQ(AlignDown(uintptr_t{0}, 4), 0);
  EXPECT_EQ(AlignDown(uintptr_t{5}, 4), 4);
  EXPECT_EQ(AlignDown(uintptr_t{4}, 4), 4);

  uint64_t val = 0;
  EXPECT_EQ(AlignDown(&val, alignof(val)), &val);
}

TEST(AlignUp, Aligns) {
  EXPECT_EQ(AlignUp(uintptr_t{0}, 4), 0);
  EXPECT_EQ(AlignUp(uintptr_t{3}, 4), 4);
  EXPECT_EQ(AlignUp(uintptr_t{5}, 4), 8);

  uint64_t val = 0;
  EXPECT_EQ(AlignUp(&val, alignof(val)), &val);
}

TEST(IsAligned, Aligned) {
  EXPECT_TRUE(IsAligned(uintptr_t{0}, 4));
  EXPECT_TRUE(IsAligned(uintptr_t{4}, 4));
  EXPECT_FALSE(IsAligned(uintptr_t{3}, 4));
  EXPECT_FALSE(IsAligned(uintptr_t{5}, 4));

  uint64_t val = 0;
  EXPECT_TRUE(IsAligned(&val, alignof(val)));
}

}  // namespace
}  // namespace cel::internal
