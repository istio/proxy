// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/qpack/qpack_decoder_stream_receiver.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace quic {
namespace test {
namespace {

// A QpackDecoderStreamReceiver::Delegate implementation that ignores all
// decoded instructions but keeps track of whether an error has been detected.
class NoOpDelegate : public QpackDecoderStreamReceiver::Delegate {
 public:
  NoOpDelegate() : error_detected_(false) {}
  ~NoOpDelegate() override = default;

  void OnInsertCountIncrement(uint64_t /*increment*/) override {}
  void OnHeaderAcknowledgement(QuicStreamId /*stream_id*/) override {}
  void OnStreamCancellation(QuicStreamId /*stream_id*/) override {}
  void OnErrorDetected(QuicErrorCode /*error_code*/,
                       absl::string_view /*error_message*/) override {
    error_detected_ = true;
  }

  bool error_detected() const { return error_detected_; }

 private:
  bool error_detected_;
};

// This fuzzer exercises QpackDecoderStreamReceiver.
void DoesNotCrash(const std::vector<uint8_t>& data) {
  NoOpDelegate delegate;
  QpackDecoderStreamReceiver receiver(&delegate);

  FuzzedDataProvider provider(data.data(), data.size());

  while (!delegate.error_detected() && provider.remaining_bytes() != 0) {
    // Process up to 64 kB fragments at a time.  Too small upper bound might not
    // provide enough coverage, too large might make fuzzing too inefficient.
    size_t fragment_size = provider.ConsumeIntegralInRange<uint16_t>(
        0, std::numeric_limits<uint16_t>::max());
    receiver.Decode(provider.ConsumeRandomLengthString(fragment_size));
  }
}
FUZZ_TEST(QpackDecoderStreamReceiverFuzzer, DoesNotCrash);

}  // namespace
}  // namespace test
}  // namespace quic
