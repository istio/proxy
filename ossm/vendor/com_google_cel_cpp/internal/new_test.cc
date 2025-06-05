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

#include "internal/new.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <tuple>

#include "internal/testing.h"

namespace cel::internal {
namespace {

using ::testing::Ge;
using ::testing::NotNull;

TEST(New, Basic) {
  void* p = New(sizeof(uint64_t));
  EXPECT_THAT(p, NotNull());
  Delete(p);
}

TEST(AlignedNew, Basic) {
  void* p =
      AlignedNew(alignof(std::max_align_t) * 2,
                 static_cast<std::align_val_t>(alignof(std::max_align_t) * 2));
  EXPECT_THAT(p, NotNull());
  AlignedDelete(p,
                static_cast<std::align_val_t>(alignof(std::max_align_t) * 2));
}

TEST(SizeReturningNew, Basic) {
  void* p;
  size_t n;
  std::tie(p, n) = SizeReturningNew(sizeof(uint64_t));
  EXPECT_THAT(p, NotNull());
  EXPECT_THAT(n, Ge(sizeof(uint64_t)));
  SizedDelete(p, n);
}

TEST(SizeReturningAlignedNew, Basic) {
  void* p;
  size_t n;
  std::tie(p, n) = SizeReturningAlignedNew(
      alignof(std::max_align_t) * 2,
      static_cast<std::align_val_t>(alignof(std::max_align_t) * 2));
  EXPECT_THAT(p, NotNull());
  EXPECT_THAT(n, Ge(alignof(std::max_align_t) * 2));
  SizedAlignedDelete(
      p, n, static_cast<std::align_val_t>(alignof(std::max_align_t) * 2));
}

}  // namespace
}  // namespace cel::internal
