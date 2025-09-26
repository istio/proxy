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

#include "common/arena_string_view.h"

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
using ::testing::SizeIs;

class ArenaStringViewTest : public ::testing::Test {
 protected:
  google::protobuf::Arena* absl_nonnull arena() { return &arena_; }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(ArenaStringViewTest, Default) {
  ArenaStringView string;
  EXPECT_THAT(string, IsEmpty());
  EXPECT_THAT(string, SizeIs(0));
  EXPECT_THAT(string, Eq(ArenaStringView()));
}

TEST_F(ArenaStringViewTest, Iterator) {
  ArenaStringView string = ArenaStringView("Hello World!", arena());
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

TEST_F(ArenaStringViewTest, ReverseIterator) {
  ArenaStringView string = ArenaStringView("Hello World!", arena());
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

TEST_F(ArenaStringViewTest, RemovePrefix) {
  ArenaStringView string = ArenaStringView("Hello World!", arena());
  string.remove_prefix(6);
  EXPECT_EQ(string, "World!");
}

TEST_F(ArenaStringViewTest, RemoveSuffix) {
  ArenaStringView string = ArenaStringView("Hello World!", arena());
  string.remove_suffix(7);
  EXPECT_EQ(string, "Hello");
}

TEST_F(ArenaStringViewTest, Equal) {
  EXPECT_THAT(ArenaStringView("1", arena()), Eq(ArenaStringView("1", arena())));
}

TEST_F(ArenaStringViewTest, NotEqual) {
  EXPECT_THAT(ArenaStringView("1", arena()), Ne(ArenaStringView("2", arena())));
}

TEST_F(ArenaStringViewTest, Less) {
  EXPECT_THAT(ArenaStringView("1", arena()), Lt(ArenaStringView("2", arena())));
}

TEST_F(ArenaStringViewTest, LessEqual) {
  EXPECT_THAT(ArenaStringView("1", arena()), Le(ArenaStringView("1", arena())));
}

TEST_F(ArenaStringViewTest, Greater) {
  EXPECT_THAT(ArenaStringView("2", arena()), Gt(ArenaStringView("1", arena())));
}

TEST_F(ArenaStringViewTest, GreaterEqual) {
  EXPECT_THAT(ArenaStringView("1", arena()), Ge(ArenaStringView("1", arena())));
}

TEST_F(ArenaStringViewTest, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {ArenaStringView("", arena()), ArenaStringView("Hello World!", arena()),
       ArenaStringView("How much wood could a woodchuck chuck if a "
                       "woodchuck could chuck wood?",
                       arena())}));
}

TEST_F(ArenaStringViewTest, Hash) {
  EXPECT_EQ(absl::HashOf(ArenaStringView("Hello World!", arena())),
            absl::HashOf(absl::string_view("Hello World!")));
}

}  // namespace
}  // namespace cel
