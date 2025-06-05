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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  quic::QuicFramer framer(quic::AllSupportedVersions(), quic::QuicTime::Zero(),
                          quic::Perspective::IS_SERVER,
                          quic::kQuicDefaultConnectionIdLength);
  const char* const packet_bytes = reinterpret_cast<const char*>(data);

  // Test the CryptoFramer.
  absl::string_view crypto_input(packet_bytes, size);
  std::unique_ptr<quic::CryptoHandshakeMessage> handshake_message(
      quic::CryptoFramer::ParseMessage(crypto_input));

  // Test the regular QuicFramer with the same input.
  quic::test::NoOpFramerVisitor visitor;
  framer.set_visitor(&visitor);
  quic::QuicEncryptedPacket packet(packet_bytes, size);
  framer.ProcessPacket(packet);

  return 0;
}
