// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "absl/functional/overload.h"
#include "quiche/quic/core/qpack/qpack_decoder_stream_sender.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace quic {
namespace test {

namespace {

class FuzzAction {
 public:
  struct InsertCountIncrement {
    uint64_t increment;
  };
  struct HeaderAcknowledgement {
    QuicStreamId stream_id;
  };
  struct StreamCancellation {
    QuicStreamId stream_id;
  };
  struct Flush {};

  using Variant = std::variant<InsertCountIncrement, HeaderAcknowledgement,
                               StreamCancellation, Flush>;
};


// This fuzzer exercises QpackDecoderStreamSender.
void DoesNotCrash(const std::vector<FuzzAction::Variant>& inputs) {
  NoopQpackStreamSenderDelegate delegate;
  QpackDecoderStreamSender sender;
  sender.set_qpack_stream_sender_delegate(&delegate);

  for (const auto& input : inputs) {
    std::visit(absl::Overload{
                   [&](const FuzzAction::InsertCountIncrement& increment) {
                     sender.SendInsertCountIncrement(increment.increment);
                   },
                   [&](const FuzzAction::HeaderAcknowledgement& ack) {
                     sender.SendHeaderAcknowledgement(ack.stream_id);
                   },
                   [&](const FuzzAction::StreamCancellation& cancel) {
                     sender.SendStreamCancellation(cancel.stream_id);
                   },
                   [&](const FuzzAction::Flush&) { sender.Flush(); },
               },
               input);
  }

  sender.Flush();
}
FUZZ_TEST(QpackDecoderStreamSenderFuzzer, DoesNotCrash);

}  // namespace
}  // namespace test
}  // namespace quic
