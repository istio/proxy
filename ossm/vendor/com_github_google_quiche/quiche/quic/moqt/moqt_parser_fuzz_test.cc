// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>

#include "absl/strings/string_view.h"
#include "quiche/quic/moqt/moqt_parser.h"
#include "quiche/quic/moqt/test_tools/moqt_parser_test_visitor.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/test_tools/in_memory_stream.h"

namespace moqt::test {
namespace {

void MoqtControlParserNeverCrashes(bool is_data_stream, bool uses_web_transport,
                                   absl::string_view stream_data, bool fin) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtParserTestVisitor visitor(/*enable_logging=*/false);

  MoqtControlParser control_parser(uses_web_transport, &stream, visitor);
  MoqtDataParser data_parser(&stream, &visitor);

  if (is_data_stream) {
    stream.Receive(stream_data, /*fin=*/fin);
    data_parser.ReadAllData();
  } else {
    stream.Receive(stream_data, /*fin=*/false);
    control_parser.ReadAndDispatchMessages();
  }
}

FUZZ_TEST(MoqtParserTest, MoqtControlParserNeverCrashes)
    .WithDomains(fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
                 fuzztest::Arbitrary<std::string>(),
                 fuzztest::Arbitrary<bool>());

// Regression test for b/446307507.
TEST(MoqtParserTest,
     MoqtControlParserNeverCrashesRegressionQuicTimeFromMillisecondsOverflow) {
  static constexpr auto kStreamData = std::to_array<char>({
      0x02, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x25, 0x01, 0x02,
      0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x6e, 0xc7,
      0x02, 0x61, 0x8a, 0x00, 0x00, 0x09, 0x09, 0x09, 0x80,
  });

  MoqtControlParserNeverCrashes(
      /*is_data_stream=*/false,
      /*uses_web_transport=*/false,
      /*stream_data=*/std::string(kStreamData.begin(), kStreamData.end()),
      /*fin=*/true);
}

}  // namespace
}  // namespace moqt::test
