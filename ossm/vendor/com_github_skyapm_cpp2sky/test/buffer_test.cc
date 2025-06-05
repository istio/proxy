// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/utils/buffer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/memory/memory.h"

namespace cpp2sky {

TEST(BufferTest, Basic) {
  ValueBuffer<int> buffer;
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);

  buffer.push_back(1);
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.size(), 1);

  buffer.push_back(2);
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.size(), 2);

  auto value = buffer.pop_front();
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), 1);
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.size(), 1);

  value = buffer.pop_front();
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), 2);
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);

  value = buffer.pop_front();
  EXPECT_FALSE(value.has_value());
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);
}

}  // namespace cpp2sky
