// Copyright 2025 Google LLC
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

#include "internal/empty_descriptors.h"

#include "internal/testing.h"

namespace cel::internal {
namespace {

using ::testing::NotNull;

TEST(GetEmptyDefaultInstance, Empty) {
  const auto* empty = GetEmptyDefaultInstance();
  ASSERT_THAT(empty, NotNull());
  EXPECT_EQ(empty->GetDescriptor()->full_name(), "google.protobuf.Empty");
  EXPECT_EQ(empty, GetEmptyDefaultInstance());
}

}  // namespace
}  // namespace cel::internal
