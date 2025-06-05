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

TEST(ListType, Default) {
  ListType list_type;
  EXPECT_EQ(list_type.element(), DynType());
}

TEST(ListType, Kind) {
  google::protobuf::Arena arena;
  EXPECT_EQ(ListType(&arena, BoolType()).kind(), ListType::kKind);
  EXPECT_EQ(Type(ListType(&arena, BoolType())).kind(), ListType::kKind);
}

TEST(ListType, Name) {
  google::protobuf::Arena arena;
  EXPECT_EQ(ListType(&arena, BoolType()).name(), ListType::kName);
  EXPECT_EQ(Type(ListType(&arena, BoolType())).name(), ListType::kName);
}

TEST(ListType, DebugString) {
  google::protobuf::Arena arena;
  {
    std::ostringstream out;
    out << ListType(&arena, BoolType());
    EXPECT_EQ(out.str(), "list<bool>");
  }
  {
    std::ostringstream out;
    out << Type(ListType(&arena, BoolType()));
    EXPECT_EQ(out.str(), "list<bool>");
  }
}

TEST(ListType, Hash) {
  google::protobuf::Arena arena;
  EXPECT_EQ(absl::HashOf(ListType(&arena, BoolType())),
            absl::HashOf(ListType(&arena, BoolType())));
}

TEST(ListType, Equal) {
  google::protobuf::Arena arena;
  EXPECT_EQ(ListType(&arena, BoolType()), ListType(&arena, BoolType()));
  EXPECT_EQ(Type(ListType(&arena, BoolType())), ListType(&arena, BoolType()));
  EXPECT_EQ(ListType(&arena, BoolType()), Type(ListType(&arena, BoolType())));
  EXPECT_EQ(Type(ListType(&arena, BoolType())),
            Type(ListType(&arena, BoolType())));
}

}  // namespace
}  // namespace cel
