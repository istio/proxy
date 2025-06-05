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

#include "internal/copy_on_write.h"

#include <cstdint>

#include "internal/testing.h"

namespace cel::internal {
namespace {

TEST(CopyOnWrite, Basic) {
  CopyOnWrite<int32_t> original;
  EXPECT_EQ(&original.mutable_get(), &original.get());
  {
    auto duplicate = original;
    EXPECT_EQ(&duplicate.get(), &original.get());
    EXPECT_NE(&duplicate.mutable_get(), &original.get());
  }
  EXPECT_EQ(&original.mutable_get(), &original.get());
}

}  // namespace
}  // namespace cel::internal
