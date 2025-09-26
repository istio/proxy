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

#include "common/arena_string.h"

#include "absl/base/nullability.h"
#include "absl/hash/hash.h"
#include "absl/hash/hash_testing.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Lt;
using ::testing::Ne;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;

class ArenaStringTest : public ::testing::Test {
 protected:
  google::protobuf::Arena* absl_nonnull arena() { return &arena_; }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(ArenaStringTest, Default) {
  ArenaString string;
  EXPECT_THAT(string, IsEmpty());
  EXPECT_THAT(string, SizeIs(0));
  EXPECT_THAT(string, Eq(ArenaString()));
}

TEST_F(ArenaStringTest, Small) {
  static constexpr absl::string_view kSmall = "Hello World!";

  ArenaString string(kSmall, arena());
  EXPECT_THAT(string, Not(IsEmpty()));
  EXPECT_THAT(string, SizeIs(kSmall.size()));
  EXPECT_THAT(string.data(), NotNull());
  EXPECT_THAT(string, kSmall);
}

TEST_F(ArenaStringTest, Large) {
  static constexpr absl::string_view kLarge =
      "This string is larger than the inline storage!";

  ArenaString string(kLarge, arena());
  EXPECT_THAT(string, Not(IsEmpty()));
  EXPECT_THAT(string, SizeIs(kLarge.size()));
  EXPECT_THAT(string.data(), NotNull());
  EXPECT_THAT(string, kLarge);
}

TEST_F(ArenaStringTest, Iterator) {
  ArenaString string = ArenaString("Hello World!", arena());
  auto it = string.cbegin();
  EXPECT_THAT(*it++, Eq('H'));
  EXPECT_THAT(*it++, Eq('e'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq(' '));
  EXPECT_THAT(*it++, Eq('W'));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq('r'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('d'));
  EXPECT_THAT(*it++, Eq('!'));
  EXPECT_THAT(it, Eq(string.cend()));
}

TEST_F(ArenaStringTest, ReverseIterator) {
  ArenaString string = ArenaString("Hello World!", arena());
  auto it = string.crbegin();
  EXPECT_THAT(*it++, Eq('!'));
  EXPECT_THAT(*it++, Eq('d'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('r'));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq('W'));
  EXPECT_THAT(*it++, Eq(' '));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('e'));
  EXPECT_THAT(*it++, Eq('H'));
  EXPECT_THAT(it, Eq(string.crend()));
}

TEST_F(ArenaStringTest, RemovePrefix) {
  ArenaString string = ArenaString("Hello World!", arena());
  string.remove_prefix(6);
  EXPECT_EQ(string, "World!");
}

TEST_F(ArenaStringTest, RemoveSuffix) {
  ArenaString string = ArenaString("Hello World!", arena());
  string.remove_suffix(7);
  EXPECT_EQ(string, "Hello");
}

TEST_F(ArenaStringTest, Equal) {
  EXPECT_THAT(ArenaString("1", arena()), Eq(ArenaString("1", arena())));
}

TEST_F(ArenaStringTest, NotEqual) {
  EXPECT_THAT(ArenaString("1", arena()), Ne(ArenaString("2", arena())));
}

TEST_F(ArenaStringTest, Less) {
  EXPECT_THAT(ArenaString("1", arena()), Lt(ArenaString("2", arena())));
}

TEST_F(ArenaStringTest, LessEqual) {
  EXPECT_THAT(ArenaString("1", arena()), Le(ArenaString("1", arena())));
}

TEST_F(ArenaStringTest, Greater) {
  EXPECT_THAT(ArenaString("2", arena()), Gt(ArenaString("1", arena())));
}

TEST_F(ArenaStringTest, GreaterEqual) {
  EXPECT_THAT(ArenaString("1", arena()), Ge(ArenaString("1", arena())));
}

TEST_F(ArenaStringTest, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {ArenaString("", arena()), ArenaString("Hello World!", arena()),
       ArenaString("How much wood could a woodchuck chuck if a "
                   "woodchuck could chuck wood?",
                   arena())}));
}

TEST_F(ArenaStringTest, Hash) {
  EXPECT_EQ(absl::HashOf(ArenaString("Hello World!", arena())),
            absl::HashOf(absl::string_view("Hello World!")));
}

}  // namespace
}  // namespace cel
