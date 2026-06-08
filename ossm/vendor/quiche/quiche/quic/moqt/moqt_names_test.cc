// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_names.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_status_utils.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace moqt::test {
namespace {

using ::quiche::test::IsOkAndHolds;
using ::quiche::test::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(MoqtNamesTest, TrackNamespaceConstructors) {
  TrackNamespace name1({"foo", "bar"});
  MoqtStringTuple list({"foo", "bar"});
  absl::StatusOr<TrackNamespace> name2 =
      TrackNamespace::Create(std::move(list));
  QUICHE_ASSERT_OK(name2);
  ASSERT_EQ(name1, *name2);
  EXPECT_EQ(absl::HashOf(name1), absl::HashOf(*name2));
}

TEST(MoqtNamesTest, TrackNamespaceOrder) {
  TrackNamespace name1({"a", "b"});
  TrackNamespace name2({"a", "b", "c"});
  TrackNamespace name3({"b", "a"});
  EXPECT_LT(name1, name2);
  EXPECT_LT(name2, name3);
  EXPECT_LT(name1, name3);
}

TEST(MoqtNamesTest, TrackNamespaceInNamespace) {
  TrackNamespace name1({"a", "b"});
  TrackNamespace name2({"a", "b", "c"});
  TrackNamespace name3({"d", "b"});
  EXPECT_TRUE(name2.InNamespace(name1));
  EXPECT_FALSE(name1.InNamespace(name2));
  EXPECT_TRUE(name1.InNamespace(name1));
  EXPECT_FALSE(name2.InNamespace(name3));
}

TEST(MoqtNamesTest, TrackNamespacePushPop) {
  TrackNamespace name({"a"});
  TrackNamespace original = name;
  EXPECT_TRUE(name.AddElement("b"));
  EXPECT_TRUE(name.InNamespace(original));
  EXPECT_FALSE(original.InNamespace(name));
  EXPECT_TRUE(name.PopElement());
  EXPECT_EQ(name, original);
  EXPECT_TRUE(name.PopElement());
  EXPECT_EQ(name.number_of_elements(), 0);
  EXPECT_FALSE(name.PopElement());
}

TEST(MoqtNamesTest, TrackNamespaceToString) {
  TrackNamespace name1({"a", "b"});
  EXPECT_EQ(name1.ToString(), "a-b");

  TrackNamespace name2({"\xff\x01", "\x61"});
  EXPECT_EQ(name2.ToString(), ".ff.01-a");

  TrackNamespace name3({"a_b", "c_d?"});
  EXPECT_EQ(name3.ToString(), "a_b-c_d.3f");
}

TEST(MoqtNamesTest, FullTrackNameToString) {
  FullTrackName name1(TrackNamespace{"a", "b"}, "c");
  EXPECT_EQ(name1.ToString(), "a-b--c");
}

TEST(MoqtNamesTest, TrackNamespaceSuffixes) {
  using quiche::test::IsOkAndHolds;
  TrackNamespace name1({"a", "b"});
  TrackNamespace name2({"c", "d"});
  TrackNamespace name3({"a", "b", "c", "d"});
  EXPECT_THAT(name1.AddSuffix(name2), IsOkAndHolds(name3));
  EXPECT_THAT(name3.ExtractSuffix(name1), IsOkAndHolds(name2));
  EXPECT_THAT(name1.AddSuffix(TrackNamespace()), IsOkAndHolds(name1));
  EXPECT_THAT(TrackNamespace().AddSuffix(name1), IsOkAndHolds(name1));
  EXPECT_THAT(name1.ExtractSuffix(TrackNamespace()), IsOkAndHolds(name1));
  EXPECT_THAT(name1.ExtractSuffix(name1), IsOkAndHolds(TrackNamespace()));
  TrackNamespace name4({"c", "b"});
  EXPECT_EQ(name1.ExtractSuffix(name4).status(),
            absl::InvalidArgumentError("Prefix is not in namespace"));
}

TEST(MoqtNamesTest, TooManyNamespaceElements) {
  // 32 elements work.
  absl::StatusOr<TrackNamespace> name1 = TrackNamespace::Create(MoqtStringTuple(
      {"a", "b", "c", "d", "e",  "f",  "g",  "h",  "i",  "j", "k",
       "l", "m", "n", "o", "p",  "q",  "r",  "s",  "t",  "u", "v",
       "w", "x", "y", "z", "aa", "bb", "cc", "dd", "ee", "ff"}));
  QUICHE_ASSERT_OK(name1);
  EXPECT_FALSE(name1->AddElement("a"));
  EXPECT_EQ(name1->number_of_elements(), kMaxNamespaceElements);

  // 33 elements fail.
  absl::StatusOr<TrackNamespace> name2 = TrackNamespace::Create(MoqtStringTuple(
      {"a", "b", "c", "d", "e",  "f",  "g",  "h",  "i",  "j",  "k",
       "l", "m", "n", "o", "p",  "q",  "r",  "s",  "t",  "u",  "v",
       "w", "x", "y", "z", "aa", "bb", "cc", "dd", "ee", "ff", "gg"}));
  EXPECT_THAT(name2.status(), StatusIs(absl::StatusCode::kOutOfRange,
                                       HasSubstr("33 elements")));
}

TEST(MoqtNamesTest, FullTrackNameTooLong) {
  char raw_name[kMaxFullTrackNameSize + 1];
  absl::string_view track_namespace(raw_name, kMaxFullTrackNameSize);
  // Adding an element takes it over the length limit.
  TrackNamespace max_length_namespace({track_namespace});
  EXPECT_FALSE(max_length_namespace.AddElement("f"));
  // Constructing a FullTrackName where the name brings it over the length
  // limit.
  EXPECT_QUICHE_BUG(FullTrackName(max_length_namespace.tuple()[0], "f"),
                    "Constructing a Full Track Name that is too large.");
  // The namespace is too long by itself..
  absl::string_view big_namespace(raw_name, kMaxFullTrackNameSize + 1);
  EXPECT_QUICHE_BUG(TrackNamespace({big_namespace}),
                    "TrackNamspace constructor");
}

absl::StatusOr<MoqtStringTuple> ParseNamespace(absl::string_view input) {
  absl::StatusOr<TrackNamespace> ns = TrackNamespace::Parse(input);
  QUICHE_RETURN_IF_ERROR(ns.status());
  return ns->tuple();
}

TEST(MoqtNamesTest, ParseNamespace) {
  EXPECT_THAT(ParseNamespace(""), IsOkAndHolds(ElementsAre()));
  EXPECT_THAT(ParseNamespace("foo"), IsOkAndHolds(ElementsAre("foo")));
  EXPECT_THAT(ParseNamespace("foo-bar"),
              IsOkAndHolds(ElementsAre("foo", "bar")));
  EXPECT_THAT(ParseNamespace("foo-bar--test"),
              IsOkAndHolds(ElementsAre("foo", "bar", "", "test")));
  EXPECT_THAT(ParseNamespace("foo-.ff"),
              IsOkAndHolds(ElementsAre("foo", "\xff")));
  EXPECT_THAT(ParseNamespace("foo-.f"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Incomplete escape sequence"));
  EXPECT_THAT(
      ParseNamespace("foo-.zz"),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("Invalid hex")));
  EXPECT_THAT(
      ParseNamespace("foo-.FF"),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("Invalid hex")));
  EXPECT_THAT(
      ParseNamespace("foo-.0z"),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("Invalid hex")));
  EXPECT_THAT(ParseNamespace("foo-.61"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Hex-encoding a safe character")));
  EXPECT_THAT(
      ParseNamespace("................"),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("Invalid hex")));
  EXPECT_THAT(ParseNamespace("foo-\xff"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid character 0xff")));
  std::string long_string(2 * kMaxFullTrackNameSize, 'a');
  EXPECT_THAT(
      ParseNamespace(long_string),
      StatusIs(absl::StatusCode::kOutOfRange, "Maximum tuple size exceeded"));
}

TEST(MoqtNamesTest, ParseFullTrackName) {
  EXPECT_THAT(FullTrackName::Parse("foo-bar--test"),
              IsOkAndHolds(FullTrackName({"foo", "bar"}, "test")));
  EXPECT_THAT(FullTrackName::Parse("foo--bar--test"),
              IsOkAndHolds(FullTrackName({"foo", "", "bar"}, "test")));
  EXPECT_THAT(FullTrackName::Parse("--test"),
              IsOkAndHolds(FullTrackName({}, "test")));
  EXPECT_THAT(
      FullTrackName::Parse("a-b-c-d-e-f"),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("must use --")));
  EXPECT_THAT(FullTrackName::Parse("foo-bar"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("missing elements")));
}

void FuzzParseNamespace(absl::string_view encoded_namespace) {
  (void)TrackNamespace::Parse(encoded_namespace);
}

void FuzzParseFullTrackName(absl::string_view encoded_track_name) {
  (void)FullTrackName::Parse(encoded_track_name);
}

void SerializeAndParseNamespace(const std::vector<std::string>& elements) {
  if (elements.empty() || elements[0].empty()) {
    return;
  }

  TrackNamespace ns;
  for (const std::string& element : elements) {
    if (!ns.AddElement(element)) {
      return;
    }
  }
  absl::StatusOr<TrackNamespace> round_trip_result =
      TrackNamespace::Parse(ns.ToString());
  QUICHE_ASSERT_OK(round_trip_result.status());
  EXPECT_EQ(ns, *round_trip_result);
}

void ParseAndSerializeNamespace(absl::string_view encoded_namespace) {
  absl::StatusOr<TrackNamespace> ns = TrackNamespace::Parse(encoded_namespace);
  if (!ns.ok()) {
    return;
  }
  EXPECT_EQ(ns->ToString(), encoded_namespace);
}

FUZZ_TEST(MoqtNamesTest, FuzzParseNamespace);
FUZZ_TEST(MoqtNamesTest, FuzzParseFullTrackName);
FUZZ_TEST(MoqtNamesTest, SerializeAndParseNamespace);
FUZZ_TEST(MoqtNamesTest, ParseAndSerializeNamespace);

}  // namespace
}  // namespace moqt::test
