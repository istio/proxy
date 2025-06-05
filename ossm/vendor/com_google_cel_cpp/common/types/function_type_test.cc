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

TEST(FunctionType, Kind) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FunctionType(&arena, DynType{}, {BytesType()}).kind(),
            FunctionType::kKind);
  EXPECT_EQ(Type(FunctionType(&arena, DynType{}, {BytesType()})).kind(),
            FunctionType::kKind);
}

TEST(FunctionType, Name) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FunctionType(&arena, DynType{}, {BytesType()}).name(), "function");
  EXPECT_EQ(Type(FunctionType(&arena, DynType{}, {BytesType()})).name(),
            "function");
}

TEST(FunctionType, DebugString) {
  google::protobuf::Arena arena;
  {
    std::ostringstream out;
    out << FunctionType(&arena, DynType{}, {BytesType()});
    EXPECT_EQ(out.str(), "(bytes) -> dyn");
  }
  {
    std::ostringstream out;
    out << Type(FunctionType(&arena, DynType{}, {BytesType()}));
    EXPECT_EQ(out.str(), "(bytes) -> dyn");
  }
}

TEST(FunctionType, Hash) {
  google::protobuf::Arena arena;
  EXPECT_EQ(absl::HashOf(FunctionType(&arena, DynType{}, {BytesType()})),
            absl::HashOf(FunctionType(&arena, DynType{}, {BytesType()})));
}

TEST(FunctionType, Equal) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FunctionType(&arena, DynType{}, {BytesType()}),
            FunctionType(&arena, DynType{}, {BytesType()}));
  EXPECT_EQ(Type(FunctionType(&arena, DynType{}, {BytesType()})),
            FunctionType(&arena, DynType{}, {BytesType()}));
  EXPECT_EQ(FunctionType(&arena, DynType{}, {BytesType()}),
            Type(FunctionType(&arena, DynType{}, {BytesType()})));
  EXPECT_EQ(Type(FunctionType(&arena, DynType{}, {BytesType()})),
            Type(FunctionType(&arena, DynType{}, {BytesType()})));
}

}  // namespace
}  // namespace cel
