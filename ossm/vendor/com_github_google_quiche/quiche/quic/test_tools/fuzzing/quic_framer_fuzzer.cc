// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/crypto_framer.h"
#include "quiche/quic/core/crypto/crypto_handshake_message.h"
#include "quiche/quic/core/quic_framer.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace quic {
namespace {
void DoesNotCrash(absl::string_view data) {
  QuicFramer framer(AllSupportedVersions(), QuicTime::Zero(),
                    Perspective::IS_SERVER, kQuicDefaultConnectionIdLength);

  // Test the CryptoFramer.
  std::unique_ptr<CryptoHandshakeMessage> handshake_message(
      CryptoFramer::ParseMessage(data));

  // Test the regular QuicFramer with the same input.
  test::NoOpFramerVisitor visitor;
  framer.set_visitor(&visitor);
  QuicEncryptedPacket packet(data.data(), data.length());
  framer.ProcessPacket(packet);
}
FUZZ_TEST(QuicFramerFuzzer, DoesNotCrash);

}  // namespace
}  // namespace quic
