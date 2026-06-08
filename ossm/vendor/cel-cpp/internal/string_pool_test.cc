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

#include "internal/string_pool.h"

#include "absl/strings/string_view.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::internal {
namespace {

TEST(StringPool, EmptyString) {
  google::protobuf::Arena arena;
  StringPool string_pool(&arena);
  absl::string_view interned_string = string_pool.InternString("");
  EXPECT_EQ(interned_string.data(), string_pool.InternString("").data());
}

TEST(StringPool, InternString) {
  google::protobuf::Arena arena;
  StringPool string_pool(&arena);
  absl::string_view interned_string = string_pool.InternString("Hello, world!");
  EXPECT_EQ(interned_string.data(),
            string_pool.InternString("Hello, world!").data());
}

}  // namespace
}  // namespace cel::internal
