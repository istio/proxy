// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_stream_priority.h"

#include <optional>

#include "quiche/quic/core/quic_types.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic::test {

TEST(HttpStreamPriority, DefaultConstructed) {
  HttpStreamPriority priority;

  EXPECT_EQ(HttpStreamPriority::kDefaultUrgency, priority.urgency);
  EXPECT_EQ(HttpStreamPriority::kDefaultIncremental, priority.incremental);
}

TEST(HttpStreamPriority, Equals) {
  EXPECT_EQ((HttpStreamPriority()),
            (HttpStreamPriority{HttpStreamPriority::kDefaultUrgency,
                                HttpStreamPriority::kDefaultIncremental}));
  EXPECT_EQ((HttpStreamPriority{5, true}), (HttpStreamPriority{5, true}));
  EXPECT_EQ((HttpStreamPriority{2, false}), (HttpStreamPriority{2, false}));
  EXPECT_EQ((HttpStreamPriority{11, true}), (HttpStreamPriority{11, true}));

  EXPECT_NE((HttpStreamPriority{1, true}), (HttpStreamPriority{3, true}));
  EXPECT_NE((HttpStreamPriority{4, false}), (HttpStreamPriority{4, true}));
  EXPECT_NE((HttpStreamPriority{6, true}), (HttpStreamPriority{2, false}));
  EXPECT_NE((HttpStreamPriority{12, true}), (HttpStreamPriority{9, true}));
  EXPECT_NE((HttpStreamPriority{2, false}), (HttpStreamPriority{8, false}));
}

TEST(WebTransportStreamPriority, DefaultConstructed) {
  WebTransportStreamPriority priority;

  EXPECT_EQ(priority.session_id, 0);
  EXPECT_EQ(priority.send_group_number, 0);
  EXPECT_EQ(priority.send_order, 0);
}

TEST(WebTransportStreamPriority, Equals) {
  EXPECT_EQ(WebTransportStreamPriority(),
            (WebTransportStreamPriority{0, 0, 0}));
  EXPECT_NE(WebTransportStreamPriority(),
            (WebTransportStreamPriority{1, 2, 3}));
  EXPECT_NE(WebTransportStreamPriority(),
            (WebTransportStreamPriority{0, 0, 1}));
}

TEST(QuicStreamPriority, Default) {
  EXPECT_EQ(QuicStreamPriority().type(), QuicPriorityType::kHttp);
  EXPECT_EQ(QuicStreamPriority().http(), HttpStreamPriority());
}

TEST(QuicStreamPriority, Equals) {
  EXPECT_EQ(QuicStreamPriority(), QuicStreamPriority(HttpStreamPriority()));
}

TEST(QuicStreamPriority, Type) {
  EXPECT_EQ(QuicStreamPriority(HttpStreamPriority()).type(),
            QuicPriorityType::kHttp);
  EXPECT_EQ(QuicStreamPriority(WebTransportStreamPriority()).type(),
            QuicPriorityType::kWebTransport);
}

TEST(SerializePriorityFieldValueTest, SerializePriorityFieldValue) {
  // Default value is omitted.
  EXPECT_EQ("", SerializePriorityFieldValue(
                    {/* urgency = */ 3, /* incremental = */ false}));
  EXPECT_EQ("u=5", SerializePriorityFieldValue(
                       {/* urgency = */ 5, /* incremental = */ false}));
  EXPECT_EQ("i", SerializePriorityFieldValue(
                     {/* urgency = */ 3, /* incremental = */ true}));
  EXPECT_EQ("u=0, i", SerializePriorityFieldValue(
                          {/* urgency = */ 0, /* incremental = */ true}));
  // Out-of-bound value is ignored.
  EXPECT_EQ("i", SerializePriorityFieldValue(
                     {/* urgency = */ 9, /* incremental = */ true}));
}

TEST(ParsePriorityFieldValueTest, ParsePriorityFieldValue) {
  // Default values
  std::optional<HttpStreamPriority> result = ParsePriorityFieldValue("");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(3, result->urgency);
  EXPECT_FALSE(result->incremental);

  result = ParsePriorityFieldValue("i=?1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(3, result->urgency);
  EXPECT_TRUE(result->incremental);

  result = ParsePriorityFieldValue("u=5");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(5, result->urgency);
  EXPECT_FALSE(result->incremental);

  result = ParsePriorityFieldValue("u=5, i");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(5, result->urgency);
  EXPECT_TRUE(result->incremental);

  result = ParsePriorityFieldValue("i, u=1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(1, result->urgency);
  EXPECT_TRUE(result->incremental);

  // Duplicate values are allowed.
  result = ParsePriorityFieldValue("u=5, i=?1, i=?0, u=2");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(2, result->urgency);
  EXPECT_FALSE(result->incremental);

  // Unknown parameters MUST be ignored.
  result = ParsePriorityFieldValue("a=42, u=4, i=?0");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(4, result->urgency);
  EXPECT_FALSE(result->incremental);

  // Out-of-range values MUST be ignored.
  result = ParsePriorityFieldValue("u=-2, i");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(3, result->urgency);
  EXPECT_TRUE(result->incremental);

  // Values of unexpected types MUST be ignored.
  result = ParsePriorityFieldValue("u=4.2, i=\"foo\"");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(3, result->urgency);
  EXPECT_FALSE(result->incremental);

  // Values of the right type but different names are ignored.
  result = ParsePriorityFieldValue("a=4, b=?1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(3, result->urgency);
  EXPECT_FALSE(result->incremental);

  // Cannot be parsed as structured headers.
  result = ParsePriorityFieldValue("000");
  EXPECT_FALSE(result.has_value());

  // Inner list dictionary values are ignored.
  result = ParsePriorityFieldValue("a=(1 2), u=1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(1, result->urgency);
  EXPECT_FALSE(result->incremental);
}

}  // namespace quic::test
