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

#include "common/internal/shared_byte_string.h"

#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/internal/reference_count.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;

class OwningObject final : public ReferenceCounted {
 public:
  explicit OwningObject(std::string string) : string_(std::move(string)) {}

  absl::string_view owned_string() const { return string_; }

 private:
  void Finalize() noexcept override { std::string().swap(string_); }

  std::string string_;
};

TEST(SharedByteString, DefaultConstructor) {
  SharedByteString byte_string;
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), IsEmpty());
  EXPECT_THAT(byte_string.ToCord(), IsEmpty());
}

TEST(SharedByteString, StringView) {
  absl::string_view string_view = "foo";
  SharedByteString byte_string(string_view);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Not(IsEmpty()));
  EXPECT_THAT(byte_string.ToString(scratch).data(), Eq(string_view.data()));
  auto cord = byte_string.ToCord();
  EXPECT_THAT(cord, Eq("foo"));
  EXPECT_THAT(cord.Flatten().data(), Ne(string_view.data()));
}

TEST(SharedByteString, OwnedStringView) {
  auto* const owner =
      new OwningObject("----------------------------------------");
  {
    SharedByteString byte_string1(owner, owner->owned_string());
    SharedByteStringView byte_string2(byte_string1);
    SharedByteString byte_string3(byte_string2);
    std::string scratch;
    EXPECT_THAT(byte_string3.ToString(scratch), Not(IsEmpty()));
    EXPECT_THAT(byte_string3.ToString(scratch).data(),
                Eq(owner->owned_string().data()));
    auto cord = byte_string3.ToCord();
    EXPECT_THAT(cord, Eq(owner->owned_string()));
    EXPECT_THAT(cord.Flatten().data(), Eq(owner->owned_string().data()));
  }
  StrongUnref(owner);
}

TEST(SharedByteString, String) {
  SharedByteString byte_string(std::string("foo"));
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(SharedByteString, Cord) {
  SharedByteString byte_string(absl::Cord("foo"));
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(SharedByteString, CopyConstruct) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(std::string("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  EXPECT_THAT(SharedByteString(byte_string1).ToString(),
              byte_string1.ToString());
  EXPECT_THAT(SharedByteString(byte_string2).ToString(),
              byte_string2.ToString());
  EXPECT_THAT(SharedByteString(byte_string3).ToString(),
              byte_string3.ToString());
}

TEST(SharedByteString, MoveConstruct) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(std::string("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  EXPECT_THAT(SharedByteString(std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT(SharedByteString(std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT(SharedByteString(std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(SharedByteString, CopyAssign) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(std::string("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  SharedByteString byte_string;
  EXPECT_THAT((byte_string = byte_string1).ToString(), byte_string1.ToString());
  EXPECT_THAT((byte_string = byte_string2).ToString(), byte_string2.ToString());
  EXPECT_THAT((byte_string = byte_string3).ToString(), byte_string3.ToString());
}

TEST(SharedByteString, MoveAssign) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(std::string("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  SharedByteString byte_string;
  EXPECT_THAT((byte_string = std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT((byte_string = std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT((byte_string = std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(SharedByteString, Swap) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(std::string("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  SharedByteString byte_string4;
  byte_string1.swap(byte_string2);
  byte_string2.swap(byte_string3);
  byte_string2.swap(byte_string3);
  byte_string2.swap(byte_string3);
  byte_string4 = byte_string1;
  byte_string1.swap(byte_string4);
  byte_string4 = byte_string2;
  byte_string2.swap(byte_string4);
  byte_string4 = byte_string3;
  byte_string3.swap(byte_string4);
  EXPECT_THAT(byte_string1.ToString(), Eq("bar"));
  EXPECT_THAT(byte_string2.ToString(), Eq("baz"));
  EXPECT_THAT(byte_string3.ToString(), Eq("foo"));
}

TEST(SharedByteString, HashValue) {
  EXPECT_EQ(absl::HashOf(SharedByteString(absl::string_view("foo"))),
            absl::HashOf(absl::string_view("foo")));
  EXPECT_EQ(absl::HashOf(SharedByteString(absl::Cord("foo"))),
            absl::HashOf(absl::Cord("foo")));
}

TEST(SharedByteString, Equality) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(absl::string_view("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  SharedByteString byte_string4(absl::Cord("qux"));
  EXPECT_NE(byte_string1, byte_string2);
  EXPECT_NE(byte_string2, byte_string1);
  EXPECT_NE(byte_string1, byte_string3);
  EXPECT_NE(byte_string3, byte_string1);
  EXPECT_NE(byte_string1, byte_string4);
  EXPECT_NE(byte_string4, byte_string1);
  EXPECT_NE(byte_string2, byte_string3);
  EXPECT_NE(byte_string3, byte_string2);
  EXPECT_NE(byte_string3, byte_string4);
  EXPECT_NE(byte_string4, byte_string3);
}

TEST(SharedByteString, LessThan) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(absl::string_view("baz"));
  SharedByteString byte_string3(absl::Cord("bar"));
  SharedByteString byte_string4(absl::Cord("qux"));
  EXPECT_LT(byte_string2, byte_string1);
  EXPECT_LT(byte_string1, byte_string4);
  EXPECT_LT(byte_string3, byte_string4);
  EXPECT_LT(byte_string3, byte_string2);
}

TEST(SharedByteString, SharedByteStringView) {
  SharedByteString byte_string1(absl::string_view("foo"));
  SharedByteString byte_string2(std::string("bar"));
  SharedByteString byte_string3(absl::Cord("baz"));
  EXPECT_THAT(SharedByteStringView(byte_string1).ToString(), Eq("foo"));
  EXPECT_THAT(SharedByteStringView(byte_string2).ToString(), Eq("bar"));
  EXPECT_THAT(SharedByteStringView(byte_string3).ToString(), Eq("baz"));
}

TEST(SharedByteStringView, DefaultConstructor) {
  SharedByteStringView byte_string;
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), IsEmpty());
  EXPECT_THAT(byte_string.ToCord(), IsEmpty());
}

TEST(SharedByteStringView, StringView) {
  absl::string_view string_view = "foo";
  SharedByteStringView byte_string(string_view);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Not(IsEmpty()));
  EXPECT_THAT(byte_string.ToString(scratch).data(), Eq(string_view.data()));
  auto cord = byte_string.ToCord();
  EXPECT_THAT(cord, Eq("foo"));
  EXPECT_THAT(cord.Flatten().data(), Ne(string_view.data()));
}

TEST(SharedByteStringView, OwnedStringView) {
  auto* const owner =
      new OwningObject("----------------------------------------");
  {
    SharedByteString byte_string1(owner, owner->owned_string());
    SharedByteStringView byte_string2(byte_string1);
    std::string scratch;
    EXPECT_THAT(byte_string2.ToString(scratch), Not(IsEmpty()));
    EXPECT_THAT(byte_string2.ToString(scratch).data(),
                Eq(owner->owned_string().data()));
    auto cord = byte_string2.ToCord();
    EXPECT_THAT(cord, Eq(owner->owned_string()));
    EXPECT_THAT(cord.Flatten().data(), Eq(owner->owned_string().data()));
  }
  StrongUnref(owner);
}

TEST(SharedByteStringView, String) {
  std::string string("foo");
  SharedByteStringView byte_string(string);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(SharedByteStringView, Cord) {
  absl::Cord cord("foo");
  SharedByteStringView byte_string(cord);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(SharedByteStringView, CopyConstruct) {
  std::string string("bar");
  absl::Cord cord("baz");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(string);
  SharedByteStringView byte_string3(cord);
  EXPECT_THAT(SharedByteString(byte_string1).ToString(),
              byte_string1.ToString());
  EXPECT_THAT(SharedByteString(byte_string2).ToString(),
              byte_string2.ToString());
  EXPECT_THAT(SharedByteString(byte_string3).ToString(),
              byte_string3.ToString());
}

TEST(SharedByteStringView, MoveConstruct) {
  std::string string("bar");
  absl::Cord cord("baz");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(string);
  SharedByteStringView byte_string3(cord);
  EXPECT_THAT(SharedByteString(std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT(SharedByteString(std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT(SharedByteString(std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(SharedByteStringView, CopyAssign) {
  std::string string("bar");
  absl::Cord cord("baz");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(string);
  SharedByteStringView byte_string3(cord);
  SharedByteStringView byte_string;
  EXPECT_THAT((byte_string = byte_string1).ToString(), byte_string1.ToString());
  EXPECT_THAT((byte_string = byte_string2).ToString(), byte_string2.ToString());
  EXPECT_THAT((byte_string = byte_string3).ToString(), byte_string3.ToString());
}

TEST(SharedByteStringView, MoveAssign) {
  std::string string("bar");
  absl::Cord cord("baz");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(string);
  SharedByteStringView byte_string3(cord);
  SharedByteStringView byte_string;
  EXPECT_THAT((byte_string = std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT((byte_string = std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT((byte_string = std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(SharedByteStringView, Swap) {
  std::string string("bar");
  absl::Cord cord("baz");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(string);
  SharedByteStringView byte_string3(cord);
  byte_string1.swap(byte_string2);
  byte_string2.swap(byte_string3);
  EXPECT_THAT(byte_string1.ToString(), Eq("bar"));
  EXPECT_THAT(byte_string2.ToString(), Eq("baz"));
  EXPECT_THAT(byte_string3.ToString(), Eq("foo"));
}

TEST(SharedByteStringView, HashValue) {
  absl::Cord cord("foo");
  EXPECT_EQ(absl::HashOf(SharedByteStringView(absl::string_view("foo"))),
            absl::HashOf(absl::string_view("foo")));
  EXPECT_EQ(absl::HashOf(SharedByteStringView(cord)), absl::HashOf(cord));
}

TEST(SharedByteStringView, Equality) {
  absl::Cord cord1("baz");
  absl::Cord cord2("qux");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(absl::string_view("bar"));
  SharedByteStringView byte_string3(cord1);
  SharedByteStringView byte_string4(cord2);
  EXPECT_NE(byte_string1, byte_string2);
  EXPECT_NE(byte_string2, byte_string1);
  EXPECT_NE(byte_string1, byte_string3);
  EXPECT_NE(byte_string3, byte_string1);
  EXPECT_NE(byte_string1, byte_string4);
  EXPECT_NE(byte_string4, byte_string1);
  EXPECT_NE(byte_string2, byte_string3);
  EXPECT_NE(byte_string3, byte_string2);
  EXPECT_NE(byte_string3, byte_string4);
  EXPECT_NE(byte_string4, byte_string3);
}

TEST(SharedByteStringView, LessThan) {
  absl::Cord cord1("bar");
  absl::Cord cord2("qux");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(absl::string_view("baz"));
  SharedByteStringView byte_string3(cord1);
  SharedByteStringView byte_string4(cord2);
  EXPECT_LT(byte_string2, byte_string1);
  EXPECT_LT(byte_string1, byte_string4);
  EXPECT_LT(byte_string3, byte_string4);
  EXPECT_LT(byte_string3, byte_string2);
}

TEST(SharedByteStringView, SharedByteString) {
  std::string string("bar");
  absl::Cord cord("baz");
  SharedByteStringView byte_string1(absl::string_view("foo"));
  SharedByteStringView byte_string2(string);
  SharedByteStringView byte_string3(cord);
  EXPECT_THAT(SharedByteString(byte_string1).ToString(), Eq("foo"));
  EXPECT_THAT(SharedByteString(byte_string2).ToString(), Eq("bar"));
  EXPECT_THAT(SharedByteString(byte_string3).ToString(), Eq("baz"));
}

}  // namespace
}  // namespace cel::common_internal
