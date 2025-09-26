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

TEST(OpaqueType, Kind) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OpaqueType(&arena, "test.Opaque", {BytesType()}).kind(),
            OpaqueType::kKind);
  EXPECT_EQ(Type(OpaqueType(&arena, "test.Opaque", {BytesType()})).kind(),
            OpaqueType::kKind);
}

TEST(OpaqueType, Name) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OpaqueType(&arena, "test.Opaque", {BytesType()}).name(),
            "test.Opaque");
  EXPECT_EQ(Type(OpaqueType(&arena, "test.Opaque", {BytesType()})).name(),
            "test.Opaque");
}

TEST(OpaqueType, DebugString) {
  google::protobuf::Arena arena;
  {
    std::ostringstream out;
    out << OpaqueType(&arena, "test.Opaque", {BytesType()});
    EXPECT_EQ(out.str(), "test.Opaque<bytes>");
  }
  {
    std::ostringstream out;
    out << Type(OpaqueType(&arena, "test.Opaque", {BytesType()}));
    EXPECT_EQ(out.str(), "test.Opaque<bytes>");
  }
  {
    std::ostringstream out;
    out << OpaqueType(&arena, "test.Opaque", {});
    EXPECT_EQ(out.str(), "test.Opaque");
  }
}

TEST(OpaqueType, Hash) {
  google::protobuf::Arena arena;
  EXPECT_EQ(absl::HashOf(OpaqueType(&arena, "test.Opaque", {BytesType()})),
            absl::HashOf(OpaqueType(&arena, "test.Opaque", {BytesType()})));
}

TEST(OpaqueType, Equal) {
  google::protobuf::Arena arena;
  EXPECT_EQ(OpaqueType(&arena, "test.Opaque", {BytesType()}),
            OpaqueType(&arena, "test.Opaque", {BytesType()}));
  EXPECT_EQ(Type(OpaqueType(&arena, "test.Opaque", {BytesType()})),
            OpaqueType(&arena, "test.Opaque", {BytesType()}));
  EXPECT_EQ(OpaqueType(&arena, "test.Opaque", {BytesType()}),
            Type(OpaqueType(&arena, "test.Opaque", {BytesType()})));
  EXPECT_EQ(Type(OpaqueType(&arena, "test.Opaque", {BytesType()})),
            Type(OpaqueType(&arena, "test.Opaque", {BytesType()})));
}

}  // namespace
}  // namespace cel
