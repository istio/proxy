// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/qpack/qpack_encoder_stream_receiver.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"
#include "quiche/common/quiche_data_reader.h"

namespace quic {
namespace test {
namespace {

// A QpackEncoderStreamReceiver::Delegate implementation that ignores all
// decoded instructions but keeps track of whether an error has been detected.
class NoOpDelegate : public QpackEncoderStreamReceiver::Delegate {
 public:
  NoOpDelegate() : error_detected_(false) {}
  ~NoOpDelegate() override = default;

  void OnInsertWithNameReference(bool /*is_static*/, uint64_t /*name_index*/,
                                 absl::string_view /*value*/) override {}
  void OnInsertWithoutNameReference(absl::string_view /*name*/,
                                    absl::string_view /*value*/) override {}
  void OnDuplicate(uint64_t /*index*/) override {}
  void OnSetDynamicTableCapacity(uint64_t /*capacity*/) override {}
  void OnErrorDetected(QuicErrorCode /*error_code*/,
                       absl::string_view /*error_message*/) override {
    error_detected_ = true;
  }

  bool error_detected() const { return error_detected_; }

 private:
  bool error_detected_;
};

// This fuzzer exercises QpackEncoderStreamReceiver.
// Note that since string literals may be encoded with or without Huffman
// encoding, one could not expect identical encoded data if the decoded
// instructions were fed into QpackEncoderStreamSender.  Therefore there is no
// point in extending this fuzzer into a round-trip test.
void DoesNotCrash(std::string data,
                  const std::vector<uint16_t>& fragment_sizes_vector) {
  NoOpDelegate delegate;
  QpackEncoderStreamReceiver receiver(&delegate);

  quiche::QuicheDataReader reader(data);
  absl::Span<const uint16_t> fragment_sizes(fragment_sizes_vector);

  while (!reader.IsDoneReading() && !fragment_sizes.empty()) {
    if (delegate.error_detected()) {
      break;
    }

    // Process up to 64 kB fragments at a time.  Too small upper bound might not
    // provide enough coverage, too large might make fuzzing too inefficient.
    uint16_t fragment_size = fragment_sizes.front();
    fragment_sizes.remove_prefix(1);
    receiver.Decode(reader.ReadAtMost(fragment_size));
  }
}
FUZZ_TEST(QpackEncoderStreamReceiverFuzzer, DoesNotCrash);

}  // namespace

}  // namespace test
}  // namespace quic
