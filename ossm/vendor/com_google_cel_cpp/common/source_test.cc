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

#include "common/source.h"

#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Optional;

TEST(SourceRange, Default) {
  SourceRange range;
  EXPECT_EQ(range.begin, -1);
  EXPECT_EQ(range.end, -1);
}

TEST(SourceRange, Equality) {
  EXPECT_THAT((SourceRange{}), (Eq(SourceRange{})));
  EXPECT_THAT((SourceRange{0, 1}), (Ne(SourceRange{0, 0})));
}

TEST(SourceLocation, Default) {
  SourceLocation location;
  EXPECT_EQ(location.line, -1);
  EXPECT_EQ(location.column, -1);
}

TEST(SourceLocation, Equality) {
  EXPECT_THAT((SourceLocation{}), (Eq(SourceLocation{})));
  EXPECT_THAT((SourceLocation{1, 1}), (Ne(SourceLocation{1, 0})));
}

TEST(StringSource, Description) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource("c.d &&\n\t b.c.arg(10) &&\n\t test(10)", "offset-test"));

  EXPECT_THAT(source->description(), Eq("offset-test"));
}

TEST(StringSource, Content) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource("c.d &&\n\t b.c.arg(10) &&\n\t test(10)", "offset-test"));

  EXPECT_THAT(source->content().ToString(),
              Eq("c.d &&\n\t b.c.arg(10) &&\n\t test(10)"));
}

TEST(StringSource, PositionAndLocation) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource("c.d &&\n\t b.c.arg(10) &&\n\t test(10)", "offset-test"));

  EXPECT_THAT(source->line_offsets(), ElementsAre(7, 24, 35));

  auto start = source->GetPosition(SourceLocation{int32_t{1}, int32_t{2}});
  auto end = source->GetPosition(SourceLocation{int32_t{3}, int32_t{2}});
  ASSERT_TRUE(start.has_value());
  ASSERT_TRUE(end.has_value());

  EXPECT_THAT(source->GetLocation(*start),
              Optional(Eq(SourceLocation{int32_t{1}, int32_t{2}})));
  EXPECT_THAT(source->GetLocation(*end),
              Optional(Eq(SourceLocation{int32_t{3}, int32_t{2}})));
  EXPECT_THAT(source->GetLocation(-1), Eq(absl::nullopt));

  EXPECT_THAT(source->content().ToString(*start, *end),
              Eq("d &&\n\t b.c.arg(10) &&\n\t "));

  EXPECT_THAT(source->GetPosition(SourceLocation{int32_t{0}, int32_t{0}}),
              Eq(absl::nullopt));
  EXPECT_THAT(source->GetPosition(SourceLocation{int32_t{1}, int32_t{-1}}),
              Eq(absl::nullopt));
  EXPECT_THAT(source->GetPosition(SourceLocation{int32_t{4}, int32_t{0}}),
              Eq(absl::nullopt));
}

TEST(StringSource, SnippetSingle) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("hello, world", "one-line-test"));

  EXPECT_THAT(source->Snippet(1), Optional(Eq("hello, world")));
  EXPECT_THAT(source->Snippet(2), Eq(absl::nullopt));
}

TEST(StringSource, SnippetMulti) {
  ASSERT_OK_AND_ASSIGN(auto source,
                       NewSource("hello\nworld\nmy\nbub\n", "four-line-test"));

  EXPECT_THAT(source->Snippet(0), Eq(absl::nullopt));
  EXPECT_THAT(source->Snippet(1), Optional(Eq("hello")));
  EXPECT_THAT(source->Snippet(2), Optional(Eq("world")));
  EXPECT_THAT(source->Snippet(3), Optional(Eq("my")));
  EXPECT_THAT(source->Snippet(4), Optional(Eq("bub")));
  EXPECT_THAT(source->Snippet(5), Optional(Eq("")));
  EXPECT_THAT(source->Snippet(6), Eq(absl::nullopt));
}

TEST(CordSource, Description) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource(absl::Cord("c.d &&\n\t b.c.arg(10) &&\n\t test(10)"),
                "offset-test"));

  EXPECT_THAT(source->description(), Eq("offset-test"));
}

TEST(CordSource, Content) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource(absl::Cord("c.d &&\n\t b.c.arg(10) &&\n\t test(10)"),
                "offset-test"));

  EXPECT_THAT(source->content().ToString(),
              Eq("c.d &&\n\t b.c.arg(10) &&\n\t test(10)"));
}

TEST(CordSource, PositionAndLocation) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource(absl::Cord("c.d &&\n\t b.c.arg(10) &&\n\t test(10)"),
                "offset-test"));

  EXPECT_THAT(source->line_offsets(), ElementsAre(7, 24, 35));

  auto start = source->GetPosition(SourceLocation{int32_t{1}, int32_t{2}});
  auto end = source->GetPosition(SourceLocation{int32_t{3}, int32_t{2}});
  ASSERT_TRUE(start.has_value());
  ASSERT_TRUE(end.has_value());

  EXPECT_THAT(source->GetLocation(*start),
              Optional(Eq(SourceLocation{int32_t{1}, int32_t{2}})));
  EXPECT_THAT(source->GetLocation(*end),
              Optional(Eq(SourceLocation{int32_t{3}, int32_t{2}})));
  EXPECT_THAT(source->GetLocation(-1), Eq(absl::nullopt));

  EXPECT_THAT(source->content().ToString(*start, *end),
              Eq("d &&\n\t b.c.arg(10) &&\n\t "));

  EXPECT_THAT(source->GetPosition(SourceLocation{int32_t{0}, int32_t{0}}),
              Eq(absl::nullopt));
  EXPECT_THAT(source->GetPosition(SourceLocation{int32_t{1}, int32_t{-1}}),
              Eq(absl::nullopt));
  EXPECT_THAT(source->GetPosition(SourceLocation{int32_t{4}, int32_t{0}}),
              Eq(absl::nullopt));
}

TEST(CordSource, SnippetSingle) {
  ASSERT_OK_AND_ASSIGN(auto source,
                       NewSource(absl::Cord("hello, world"), "one-line-test"));

  EXPECT_THAT(source->Snippet(1), Optional(Eq("hello, world")));
  EXPECT_THAT(source->Snippet(2), Eq(absl::nullopt));
}

TEST(CordSource, SnippetMulti) {
  ASSERT_OK_AND_ASSIGN(
      auto source,
      NewSource(absl::Cord("hello\nworld\nmy\nbub\n"), "four-line-test"));

  EXPECT_THAT(source->Snippet(0), Eq(absl::nullopt));
  EXPECT_THAT(source->Snippet(1), Optional(Eq("hello")));
  EXPECT_THAT(source->Snippet(2), Optional(Eq("world")));
  EXPECT_THAT(source->Snippet(3), Optional(Eq("my")));
  EXPECT_THAT(source->Snippet(4), Optional(Eq("bub")));
  EXPECT_THAT(source->Snippet(5), Optional(Eq("")));
  EXPECT_THAT(source->Snippet(6), Eq(absl::nullopt));
}

TEST(Source, DisplayErrorLocationBasic) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("'Hello' +\n  'world'"));

  SourceLocation location{/*line=*/2, /*column=*/3};

  EXPECT_EQ(source->DisplayErrorLocation(location),
            "\n |   'world'"
            "\n | ...^");
}

TEST(Source, DisplayErrorLocationOutOfRange) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("'Hello world!'"));

  SourceLocation location{/*line=*/3, /*column=*/3};

  EXPECT_EQ(source->DisplayErrorLocation(location), "");
}

TEST(Source, DisplayErrorLocationTabsShortened) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("'Hello' +\n\t\t'world!'"));

  SourceLocation location{/*line=*/2, /*column=*/4};

  EXPECT_EQ(source->DisplayErrorLocation(location),
            "\n |   'world!'"
            "\n | ....^");
}

TEST(Source, DisplayErrorLocationFullWidth) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("'Ｈｅｌｌｏ'"));

  SourceLocation location{/*line=*/1, /*column=*/2};

  EXPECT_EQ(source->DisplayErrorLocation(location),
            "\n | 'Ｈｅｌｌｏ'"
            "\n | .．＾");
}

}  // namespace
}  // namespace cel
