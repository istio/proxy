// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/decoder/payload_decoders/priority_payload_decoder.h"

#include <stddef.h>

#include "quiche/http2/core/http2_constants.h"
#include "quiche/http2/decoder/http2_frame_decoder_listener.h"
#include "quiche/http2/test_tools/frame_parts.h"
#include "quiche/http2/test_tools/frame_parts_collector.h"
#include "quiche/http2/test_tools/http2_frame_builder.h"
#include "quiche/http2/test_tools/http2_random.h"
#include "quiche/http2/test_tools/http2_structures_test_util.h"
#include "quiche/http2/test_tools/payload_decoder_base_test_util.h"
#include "quiche/http2/test_tools/random_decoder_test_base.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace test {

class PriorityPayloadDecoderPeer {
 public:
  static constexpr Http2FrameType FrameType() {
    return Http2FrameType::PRIORITY;
  }

  // Returns the mask of flags that affect the decoding of the payload (i.e.
  // flags that that indicate the presence of certain fields or padding).
  static constexpr uint8_t FlagsAffectingPayloadDecoding() { return 0; }
};

namespace {

struct Listener : public FramePartsCollector {
  void OnPriorityFrame(const Http2FrameHeader& header,
                       const Http2PriorityFields& priority_fields) override {
    QUICHE_VLOG(1) << "OnPriority: " << header << "; " << priority_fields;
    StartAndEndFrame(header)->OnPriorityFrame(header, priority_fields);
  }

  void OnFrameSizeError(const Http2FrameHeader& header) override {
    QUICHE_VLOG(1) << "OnFrameSizeError: " << header;
    FrameError(header)->OnFrameSizeError(header);
  }
};

class PriorityPayloadDecoderTest
    : public AbstractPayloadDecoderTest<PriorityPayloadDecoder,
                                        PriorityPayloadDecoderPeer, Listener> {
 protected:
  Http2PriorityFields RandPriorityFields() {
    Http2PriorityFields fields;
    test::Randomize(&fields, RandomPtr());
    return fields;
  }
};

// Confirm we get an error if the payload is not the correct size to hold
// exactly one Http2PriorityFields.
TEST_F(PriorityPayloadDecoderTest, WrongSize) {
  auto approve_size = [](size_t size) {
    return size != Http2PriorityFields::EncodedSize();
  };
  Http2FrameBuilder fb;
  fb.Append(RandPriorityFields());
  fb.Append(RandPriorityFields());
  EXPECT_TRUE(VerifyDetectsFrameSizeError(0, fb.buffer(), approve_size));
}

TEST_F(PriorityPayloadDecoderTest, VariousPayloads) {
  for (int n = 0; n < 100; ++n) {
    Http2PriorityFields fields = RandPriorityFields();
    Http2FrameBuilder fb;
    fb.Append(fields);
    Http2FrameHeader header(fb.size(), Http2FrameType::PRIORITY, RandFlags(),
                            RandStreamId());
    set_frame_header(header);
    FrameParts expected(header);
    expected.SetOptPriority(fields);
    EXPECT_TRUE(DecodePayloadAndValidateSeveralWays(fb.buffer(), expected));
  }
}

}  // namespace
}  // namespace test
}  // namespace http2
