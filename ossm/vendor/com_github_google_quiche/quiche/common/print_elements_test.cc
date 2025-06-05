// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/print_elements.h"

#include <deque>
#include <list>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/common/platform/api/quiche_test.h"

using quic::QuicIetfTransportErrorCodes;

namespace quiche {
namespace test {
namespace {

TEST(PrintElementsTest, Empty) {
  std::vector<std::string> empty{};
  EXPECT_EQ("{}", PrintElements(empty));
}

TEST(PrintElementsTest, StdContainers) {
  std::vector<std::string> one{"foo"};
  EXPECT_EQ("{foo}", PrintElements(one));

  std::list<std::string> two{"foo", "bar"};
  EXPECT_EQ("{foo, bar}", PrintElements(two));

  std::deque<absl::string_view> three{"foo", "bar", "baz"};
  EXPECT_EQ("{foo, bar, baz}", PrintElements(three));
}

// QuicIetfTransportErrorCodes has a custom operator<<() override.
TEST(PrintElementsTest, CustomPrinter) {
  std::vector<QuicIetfTransportErrorCodes> empty{};
  EXPECT_EQ("{}", PrintElements(empty));

  std::list<QuicIetfTransportErrorCodes> one{
      QuicIetfTransportErrorCodes::NO_IETF_QUIC_ERROR};
  EXPECT_EQ("{NO_IETF_QUIC_ERROR}", PrintElements(one));

  std::vector<QuicIetfTransportErrorCodes> two{
      QuicIetfTransportErrorCodes::FLOW_CONTROL_ERROR,
      QuicIetfTransportErrorCodes::STREAM_LIMIT_ERROR};
  EXPECT_EQ("{FLOW_CONTROL_ERROR, STREAM_LIMIT_ERROR}", PrintElements(two));

  std::list<QuicIetfTransportErrorCodes> three{
      QuicIetfTransportErrorCodes::CONNECTION_ID_LIMIT_ERROR,
      QuicIetfTransportErrorCodes::PROTOCOL_VIOLATION,
      QuicIetfTransportErrorCodes::INVALID_TOKEN};
  EXPECT_EQ("{CONNECTION_ID_LIMIT_ERROR, PROTOCOL_VIOLATION, INVALID_TOKEN}",
            PrintElements(three));
}

}  // anonymous namespace
}  // namespace test
}  // namespace quiche
