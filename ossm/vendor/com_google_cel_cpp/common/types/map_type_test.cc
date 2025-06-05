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

TEST(MapType, Default) {
  MapType map_type;
  EXPECT_EQ(map_type.key(), DynType());
  EXPECT_EQ(map_type.value(), DynType());
}

TEST(MapType, Kind) {
  google::protobuf::Arena arena;
  EXPECT_EQ(MapType(&arena, StringType(), BytesType()).kind(), MapType::kKind);
  EXPECT_EQ(Type(MapType(&arena, StringType(), BytesType())).kind(),
            MapType::kKind);
}

TEST(MapType, Name) {
  google::protobuf::Arena arena;
  EXPECT_EQ(MapType(&arena, StringType(), BytesType()).name(), MapType::kName);
  EXPECT_EQ(Type(MapType(&arena, StringType(), BytesType())).name(),
            MapType::kName);
}

TEST(MapType, DebugString) {
  google::protobuf::Arena arena;
  {
    std::ostringstream out;
    out << MapType(&arena, StringType(), BytesType());
    EXPECT_EQ(out.str(), "map<string, bytes>");
  }
  {
    std::ostringstream out;
    out << Type(MapType(&arena, StringType(), BytesType()));
    EXPECT_EQ(out.str(), "map<string, bytes>");
  }
}

TEST(MapType, Hash) {
  google::protobuf::Arena arena;
  EXPECT_EQ(absl::HashOf(MapType(&arena, StringType(), BytesType())),
            absl::HashOf(MapType(&arena, StringType(), BytesType())));
}

TEST(MapType, Equal) {
  google::protobuf::Arena arena;
  EXPECT_EQ(MapType(&arena, StringType(), BytesType()),
            MapType(&arena, StringType(), BytesType()));
  EXPECT_EQ(Type(MapType(&arena, StringType(), BytesType())),
            MapType(&arena, StringType(), BytesType()));
  EXPECT_EQ(MapType(&arena, StringType(), BytesType()),
            Type(MapType(&arena, StringType(), BytesType())));
  EXPECT_EQ(Type(MapType(&arena, StringType(), BytesType())),
            Type(MapType(&arena, StringType(), BytesType())));
}

}  // namespace
}  // namespace cel
