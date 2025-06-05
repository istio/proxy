// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A test of roundtrips through the encoder and decoder.

#include <stddef.h>

#include <string>
#include <tuple>

#include "absl/strings/string_view.h"
#include "quiche/http2/decoder/decode_buffer.h"
#include "quiche/http2/decoder/decode_status.h"
#include "quiche/http2/hpack/huffman/hpack_huffman_decoder.h"
#include "quiche/http2/hpack/huffman/hpack_huffman_encoder.h"
#include "quiche/http2/test_tools/random_decoder_test_base.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_text_utils.h"

using ::testing::AssertionSuccess;
using ::testing::Combine;
using ::testing::Range;
using ::testing::Values;

namespace http2 {
namespace test {
namespace {

std::string GenAsciiNonControlSet() {
  std::string s;
  const char space = ' ';  // First character after the control characters: 0x20
  const char del = 127;    // First character after the non-control characters.
  for (char c = space; c < del; ++c) {
    s.push_back(c);
  }
  return s;
}

class HpackHuffmanTranscoderTest : public RandomDecoderTest {
 protected:
  HpackHuffmanTranscoderTest()
      : ascii_non_control_set_(GenAsciiNonControlSet()) {
    // The decoder may return true, and its accumulator may be empty, at
    // many boundaries while decoding, and yet the whole string hasn't
    // been decoded.
    stop_decode_on_done_ = false;
  }

  DecodeStatus StartDecoding(DecodeBuffer* b) override {
    input_bytes_seen_ = 0;
    output_buffer_.clear();
    decoder_.Reset();
    return ResumeDecoding(b);
  }

  DecodeStatus ResumeDecoding(DecodeBuffer* b) override {
    input_bytes_seen_ += b->Remaining();
    absl::string_view sp(b->cursor(), b->Remaining());
    if (decoder_.Decode(sp, &output_buffer_)) {
      b->AdvanceCursor(b->Remaining());
      // Successfully decoded (or buffered) the bytes in absl::string_view.
      EXPECT_LE(input_bytes_seen_, input_bytes_expected_);
      // Have we reached the end of the encoded string?
      if (input_bytes_expected_ == input_bytes_seen_) {
        if (decoder_.InputProperlyTerminated()) {
          return DecodeStatus::kDecodeDone;
        } else {
          return DecodeStatus::kDecodeError;
        }
      }
      return DecodeStatus::kDecodeInProgress;
    }
    return DecodeStatus::kDecodeError;
  }

  AssertionResult TranscodeAndValidateSeveralWays(
      absl::string_view plain, absl::string_view expected_huffman) {
    size_t encoded_size = HuffmanSize(plain);
    std::string encoded;
    HuffmanEncode(plain, encoded_size, &encoded);
    HTTP2_VERIFY_EQ(encoded_size, encoded.size());
    if (!expected_huffman.empty() || plain.empty()) {
      HTTP2_VERIFY_EQ(encoded, expected_huffman);
    }
    input_bytes_expected_ = encoded.size();
    auto validator = [plain, this]() -> AssertionResult {
      HTTP2_VERIFY_EQ(output_buffer_.size(), plain.size());
      HTTP2_VERIFY_EQ(output_buffer_, plain);
      return AssertionSuccess();
    };
    DecodeBuffer db(encoded);
    bool return_non_zero_on_first = false;
    return DecodeAndValidateSeveralWays(&db, return_non_zero_on_first,
                                        ValidateDoneAndEmpty(validator));
  }

  AssertionResult TranscodeAndValidateSeveralWays(absl::string_view plain) {
    return TranscodeAndValidateSeveralWays(plain, "");
  }

  std::string RandomAsciiNonControlString(int length) {
    return Random().RandStringWithAlphabet(length, ascii_non_control_set_);
  }

  std::string RandomBytes(int length) { return Random().RandString(length); }

  const std::string ascii_non_control_set_;
  HpackHuffmanDecoder decoder_;
  std::string output_buffer_;
  size_t input_bytes_seen_;
  size_t input_bytes_expected_;
};

TEST_F(HpackHuffmanTranscoderTest, RoundTripRandomAsciiNonControlString) {
  for (size_t length = 0; length != 20; length++) {
    const std::string s = RandomAsciiNonControlString(length);
    ASSERT_TRUE(TranscodeAndValidateSeveralWays(s))
        << "Unable to decode:\n\n"
        << quiche::QuicheTextUtils::HexDump(s) << "\n\noutput_buffer_:\n"
        << quiche::QuicheTextUtils::HexDump(output_buffer_);
  }
}

TEST_F(HpackHuffmanTranscoderTest, RoundTripRandomBytes) {
  for (size_t length = 0; length != 20; length++) {
    const std::string s = RandomBytes(length);
    ASSERT_TRUE(TranscodeAndValidateSeveralWays(s))
        << "Unable to decode:\n\n"
        << quiche::QuicheTextUtils::HexDump(s) << "\n\noutput_buffer_:\n"
        << quiche::QuicheTextUtils::HexDump(output_buffer_);
  }
}

// Two parameters: decoder choice, and the character to round-trip.
class HpackHuffmanTranscoderAdjacentCharTest
    : public HpackHuffmanTranscoderTest,
      public testing::WithParamInterface<int> {
 protected:
  HpackHuffmanTranscoderAdjacentCharTest()
      : c_(static_cast<char>(GetParam())) {}

  const char c_;
};

INSTANTIATE_TEST_SUITE_P(HpackHuffmanTranscoderAdjacentCharTest,
                         HpackHuffmanTranscoderAdjacentCharTest, Range(0, 256));

// Test c_ adjacent to every other character, both before and after.
TEST_P(HpackHuffmanTranscoderAdjacentCharTest, RoundTripAdjacentChar) {
  std::string s;
  for (int a = 0; a < 256; ++a) {
    s.push_back(static_cast<char>(a));
    s.push_back(c_);
    s.push_back(static_cast<char>(a));
  }
  ASSERT_TRUE(TranscodeAndValidateSeveralWays(s));
}

// Two parameters: character to repeat, number of repeats.
class HpackHuffmanTranscoderRepeatedCharTest
    : public HpackHuffmanTranscoderTest,
      public testing::WithParamInterface<std::tuple<int, int>> {
 protected:
  HpackHuffmanTranscoderRepeatedCharTest()
      : c_(static_cast<char>(std::get<0>(GetParam()))),
        length_(std::get<1>(GetParam())) {}
  std::string MakeString() { return std::string(length_, c_); }

 private:
  const char c_;
  const size_t length_;
};

INSTANTIATE_TEST_SUITE_P(HpackHuffmanTranscoderRepeatedCharTest,
                         HpackHuffmanTranscoderRepeatedCharTest,
                         Combine(Range(0, 256), Values(1, 2, 3, 4, 8, 16, 32)));

TEST_P(HpackHuffmanTranscoderRepeatedCharTest, RoundTripRepeatedChar) {
  ASSERT_TRUE(TranscodeAndValidateSeveralWays(MakeString()));
}

}  // namespace
}  // namespace test
}  // namespace http2
