// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "quiche/http2/core/recording_headers_handler.h"
#include "quiche/http2/hpack/hpack_decoder_adapter.h"
#include "quiche/http2/hpack/hpack_encoder.h"
#include "quiche/http2/test_tools/http2_random.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace spdy {
namespace test {

namespace {

// Supports testing with the input split at every byte boundary.
enum InputSizeParam { ALL_INPUT, ONE_BYTE, ZERO_THEN_ONE_BYTE };

class HpackRoundTripTest
    : public quiche::test::QuicheTestWithParam<InputSizeParam> {
 protected:
  void SetUp() override {
    // Use a small table size to tickle eviction handling.
    encoder_.ApplyHeaderTableSizeSetting(256);
    decoder_.ApplyHeaderTableSizeSetting(256);
  }

  bool RoundTrip(const quiche::HttpHeaderBlock& header_set) {
    std::string encoded = encoder_.EncodeHeaderBlock(header_set);

    bool success = true;
    decoder_.HandleControlFrameHeadersStart(&handler_);
    if (GetParam() == ALL_INPUT) {
      // Pass all the input to the decoder at once.
      success = decoder_.HandleControlFrameHeadersData(encoded.data(),
                                                       encoded.size());
    } else if (GetParam() == ONE_BYTE) {
      // Pass the input to the decoder one byte at a time.
      const char* data = encoded.data();
      for (size_t ndx = 0; ndx < encoded.size() && success; ++ndx) {
        success = decoder_.HandleControlFrameHeadersData(data + ndx, 1);
      }
    } else if (GetParam() == ZERO_THEN_ONE_BYTE) {
      // Pass the input to the decoder one byte at a time, but before each
      // byte pass an empty buffer.
      const char* data = encoded.data();
      for (size_t ndx = 0; ndx < encoded.size() && success; ++ndx) {
        success = (decoder_.HandleControlFrameHeadersData(data + ndx, 0) &&
                   decoder_.HandleControlFrameHeadersData(data + ndx, 1));
      }
    } else {
      ADD_FAILURE() << "Unknown param: " << GetParam();
    }

    if (success) {
      success = decoder_.HandleControlFrameHeadersComplete();
    }

    EXPECT_EQ(header_set, handler_.decoded_block());
    return success;
  }

  size_t SampleExponential(size_t mean, size_t sanity_bound) {
    return std::min<size_t>(-std::log(random_.RandDouble()) * mean,
                            sanity_bound);
  }

  http2::test::Http2Random random_;
  HpackEncoder encoder_;
  HpackDecoderAdapter decoder_;
  RecordingHeadersHandler handler_;
};

INSTANTIATE_TEST_SUITE_P(Tests, HpackRoundTripTest,
                         ::testing::Values(ALL_INPUT, ONE_BYTE,
                                           ZERO_THEN_ONE_BYTE));

TEST_P(HpackRoundTripTest, ResponseFixtures) {
  {
    quiche::HttpHeaderBlock headers;
    headers[":status"] = "302";
    headers["cache-control"] = "private";
    headers["date"] = "Mon, 21 Oct 2013 20:13:21 GMT";
    headers["location"] = "https://www.example.com";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    quiche::HttpHeaderBlock headers;
    headers[":status"] = "200";
    headers["cache-control"] = "private";
    headers["date"] = "Mon, 21 Oct 2013 20:13:21 GMT";
    headers["location"] = "https://www.example.com";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    quiche::HttpHeaderBlock headers;
    headers[":status"] = "200";
    headers["cache-control"] = "private";
    headers["content-encoding"] = "gzip";
    headers["date"] = "Mon, 21 Oct 2013 20:13:22 GMT";
    headers["location"] = "https://www.example.com";
    headers["set-cookie"] =
        "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
        " max-age=3600; version=1";
    headers["multivalue"] = std::string("foo\0bar", 7);
    EXPECT_TRUE(RoundTrip(headers));
  }
}

TEST_P(HpackRoundTripTest, RequestFixtures) {
  {
    quiche::HttpHeaderBlock headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/";
    headers[":scheme"] = "http";
    headers["cookie"] = "baz=bing; foo=bar";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    quiche::HttpHeaderBlock headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/";
    headers[":scheme"] = "http";
    headers["cache-control"] = "no-cache";
    headers["cookie"] = "foo=bar; spam=eggs";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    quiche::HttpHeaderBlock headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/index.html";
    headers[":scheme"] = "https";
    headers["custom-key"] = "custom-value";
    headers["cookie"] = "baz=bing; fizzle=fazzle; garbage";
    headers["multivalue"] = std::string("foo\0bar", 7);
    EXPECT_TRUE(RoundTrip(headers));
  }
}

TEST_P(HpackRoundTripTest, RandomizedExamples) {
  // Grow vectors of names & values, which are seeded with fixtures and then
  // expanded with dynamically generated data. Samples are taken using the
  // exponential distribution.
  std::vector<std::string> pseudo_header_names, random_header_names;
  pseudo_header_names.push_back(":authority");
  pseudo_header_names.push_back(":path");
  pseudo_header_names.push_back(":status");

  // TODO(jgraettinger): Enable "cookie" as a name fixture. Crumbs may be
  // reconstructed in any order, which breaks the simple validation used here.

  std::vector<std::string> values;
  values.push_back("/");
  values.push_back("/index.html");
  values.push_back("200");
  values.push_back("404");
  values.push_back("");
  values.push_back("baz=bing; foo=bar; garbage");
  values.push_back("baz=bing; fizzle=fazzle; garbage");

  for (size_t i = 0; i != 2000; ++i) {
    quiche::HttpHeaderBlock headers;

    // Choose a random number of headers to add, and of these a random subset
    // will be HTTP/2 pseudo headers.
    size_t header_count = 1 + SampleExponential(7, 50);
    size_t pseudo_header_count =
        std::min(header_count, 1 + SampleExponential(7, 50));
    EXPECT_LE(pseudo_header_count, header_count);
    for (size_t j = 0; j != header_count; ++j) {
      std::string name, value;
      // Pseudo headers must be added before regular headers.
      if (j < pseudo_header_count) {
        // Choose one of the defined pseudo headers at random.
        size_t name_index = random_.Uniform(pseudo_header_names.size());
        name = pseudo_header_names[name_index];
      } else {
        // Randomly reuse an existing header name, or generate a new one.
        size_t name_index = SampleExponential(20, 200);
        if (name_index >= random_header_names.size()) {
          name = random_.RandString(1 + SampleExponential(5, 30));
          // A regular header cannot begin with the pseudo header prefix ":".
          if (name[0] == ':') {
            name[0] = 'x';
          }
          random_header_names.push_back(name);
        } else {
          name = random_header_names[name_index];
        }
      }

      // Randomly reuse an existing value, or generate a new one.
      size_t value_index = SampleExponential(20, 200);
      if (value_index >= values.size()) {
        std::string newvalue =
            random_.RandString(1 + SampleExponential(15, 75));
        // Currently order is not preserved in the encoder.  In particular,
        // when a value is decomposed at \0 delimiters, its parts might get
        // encoded out of order if some but not all of them already exist in
        // the header table.  For now, avoid \0 bytes in values.
        std::replace(newvalue.begin(), newvalue.end(), '\x00', '\x01');
        values.push_back(newvalue);
        value = values.back();
      } else {
        value = values[value_index];
      }
      headers[name] = value;
    }
    EXPECT_TRUE(RoundTrip(headers));
  }
}

}  // namespace

}  // namespace test
}  // namespace spdy
