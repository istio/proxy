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
#include "google/protobuf/arena.h"

namespace cel {
namespace {

TEST(OptionalType, Default) {
  OptionalType optional_type;
  EXPECT_EQ(optional_type.GetParameter(), DynType());
}

TEST(OptionalType, Kind) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OptionalType(&arena, BoolType()).kind(), OptionalType::kKind);
  EXPECT_EQ(Type(OptionalType(&arena, BoolType())).kind(), OptionalType::kKind);
}

TEST(OptionalType, Name) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OptionalType(&arena, BoolType()).name(), OptionalType::kName);
  EXPECT_EQ(Type(OptionalType(&arena, BoolType())).name(), OptionalType::kName);
}

TEST(OptionalType, DebugString) {
  google::protobuf::Arena arena;
  {
    std::ostringstream out;
    out << OptionalType(&arena, BoolType());
    EXPECT_EQ(out.str(), "optional_type<bool>");
  }
  {
    std::ostringstream out;
    out << Type(OptionalType(&arena, BoolType()));
    EXPECT_EQ(out.str(), "optional_type<bool>");
  }
}

TEST(OptionalType, Parameter) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OptionalType(&arena, BoolType()).GetParameter(), BoolType());
}

TEST(OptionalType, Hash) {
  google::protobuf::Arena arena;
  EXPECT_EQ(absl::HashOf(OptionalType(&arena, BoolType())),
            absl::HashOf(OptionalType(&arena, BoolType())));
}

TEST(OptionalType, Equal) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OptionalType(&arena, BoolType()), OptionalType(&arena, BoolType()));
  EXPECT_EQ(Type(OptionalType(&arena, BoolType())),
            OptionalType(&arena, BoolType()));
  EXPECT_EQ(OptionalType(&arena, BoolType()),
            Type(OptionalType(&arena, BoolType())));
  EXPECT_EQ(Type(OptionalType(&arena, BoolType())),
            Type(OptionalType(&arena, BoolType())));
}

}  // namespace
}  // namespace cel
