// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_messages.h"

#include <optional>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

TEST(MoqtMessagesTest, TrackNamespaceConstructors) {
  TrackNamespace name1({"foo", "bar"});
  std::vector<absl::string_view> list = {"foo", "bar"};
  TrackNamespace name2(list);
  EXPECT_EQ(name1, name2);
  EXPECT_EQ(absl::HashOf(name1), absl::HashOf(name2));
}

TEST(MoqtMessagesTest, TrackNamespaceOrder) {
  TrackNamespace name1({"a", "b"});
  TrackNamespace name2({"a", "b", "c"});
  TrackNamespace name3({"b", "a"});
  EXPECT_LT(name1, name2);
  EXPECT_LT(name2, name3);
  EXPECT_LT(name1, name3);
}

TEST(MoqtMessagesTest, TrackNamespaceInNamespace) {
  TrackNamespace name1({"a", "b"});
  TrackNamespace name2({"a", "b", "c"});
  TrackNamespace name3({"d", "b"});
  EXPECT_TRUE(name2.InNamespace(name1));
  EXPECT_FALSE(name1.InNamespace(name2));
  EXPECT_TRUE(name1.InNamespace(name1));
  EXPECT_FALSE(name2.InNamespace(name3));
}

TEST(MoqtMessagesTest, TrackNamespacePushPop) {
  TrackNamespace name({"a"});
  TrackNamespace original = name;
  name.AddElement("b");
  EXPECT_TRUE(name.InNamespace(original));
  EXPECT_FALSE(original.InNamespace(name));
  EXPECT_TRUE(name.PopElement());
  EXPECT_EQ(name, original);
  EXPECT_FALSE(name.PopElement());
}

TEST(MoqtMessagesTest, TrackNamespaceToString) {
  TrackNamespace name1({"a", "b"});
  EXPECT_EQ(name1.ToString(), R"({"a"::"b"})");

  TrackNamespace name2({"\xff", "\x61"});
  EXPECT_EQ(name2.ToString(), R"({"\xff"::"a"})");
}

TEST(MoqtMessagesTest, FullTrackNameToString) {
  FullTrackName name1(TrackNamespace{"a", "b"}, "c");
  EXPECT_EQ(name1.ToString(), R"({"a"::"b"}::c)");
}

TEST(MoqtMessagesTest, TooManyNamespaceElements) {
  // 32 elements work.
  TrackNamespace name1({"a", "b", "c",  "d",  "e",  "f",  "g",  "h",
                        "i", "j", "k",  "l",  "m",  "n",  "o",  "p",
                        "q", "r", "s",  "t",  "u",  "v",  "w",  "x",
                        "y", "z", "aa", "bb", "cc", "dd", "ee", "ff"});
  EXPECT_TRUE(name1.IsValid());
  EXPECT_QUICHE_BUG(name1.AddElement("a"),
                    "Constructing a namespace that is too large.");
  EXPECT_EQ(name1.number_of_elements(), kMaxNamespaceElements);

  // 33 elements fail,
  TrackNamespace name2;
  EXPECT_QUICHE_BUG(
      name2 = TrackNamespace({"a",  "b",  "c",  "d",  "e",  "f", "g", "h", "i",
                              "j",  "k",  "l",  "m",  "n",  "o", "p", "q", "r",
                              "s",  "t",  "u",  "v",  "w",  "x", "y", "z", "aa",
                              "bb", "cc", "dd", "ee", "ff", "gg"}),
      "Constructing a namespace that is too large.");
  EXPECT_FALSE(name2.IsValid());
}

TEST(MoqtMessagesTest, FullTrackNameTooLong) {
  char raw_name[kMaxFullTrackNameSize + 1];
  absl::string_view track_namespace(raw_name, kMaxFullTrackNameSize);
  // Adding an element takes it over the length limit.
  TrackNamespace max_length_namespace({track_namespace});
  EXPECT_TRUE(max_length_namespace.IsValid());
  EXPECT_QUICHE_BUG(max_length_namespace.AddElement("f"),
                    "Constructing a namespace that is too large.");
  // Constructing a FullTrackName where the name brings it over the length
  // limit.
  EXPECT_QUICHE_BUG(FullTrackName(max_length_namespace, "f"),
                    "Constructing a Full Track Name that is too large.");
  // The namespace is too long by itself..
  absl::string_view big_namespace(raw_name, kMaxFullTrackNameSize + 1);
  EXPECT_QUICHE_BUG(TrackNamespace({big_namespace}),
                    "Constructing a namespace that is too large.");
}

TEST(MoqtMessagesTest, MoqtDatagramType) {
  for (bool payload : {false, true}) {
    for (bool extension : {false, true}) {
      for (bool end_of_group : {false, true}) {
        for (bool zero_object_id : {false, true}) {
          MoqtDatagramType type(payload, extension, end_of_group,
                                zero_object_id);
          EXPECT_EQ(type.has_status(),
                    !payload && (!end_of_group || !zero_object_id));
          EXPECT_EQ(type.has_extension(), extension);
          EXPECT_EQ(type.end_of_group(),
                    end_of_group && (payload || zero_object_id));
          EXPECT_EQ(type.has_object_id(),
                    !zero_object_id || (!payload && !end_of_group));
          // The constructor should always produce a valid value.
          std::optional<MoqtDatagramType> from_value =
              MoqtDatagramType::FromValue(type.value());
          EXPECT_TRUE(from_value.has_value() && type == *from_value);
        }
      }
    }
  }
}

}  // namespace
}  // namespace moqt::test
