#include "quiche/http2/adapter/test_utils.h"

#include <optional>
#include <string>
#include <utility>

#include "quiche/http2/core/spdy_framer.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

using spdy::SpdyFramer;

TEST(EqualsFrames, Empty) {
  EXPECT_THAT("", EqualsFrames(std::vector<spdy::SpdyFrameType>{}));
}

TEST(EqualsFrames, SingleFrameWithLength) {
  SpdyFramer framer{SpdyFramer::ENABLE_COMPRESSION};

  spdy::SpdyPingIR ping{511};
  EXPECT_THAT(framer.SerializeFrame(ping),
              EqualsFrames({{spdy::SpdyFrameType::PING, 8}}));

  spdy::SpdyWindowUpdateIR window_update{1, 101};
  EXPECT_THAT(framer.SerializeFrame(window_update),
              EqualsFrames({{spdy::SpdyFrameType::WINDOW_UPDATE, 4}}));

  spdy::SpdyDataIR data{3, "Some example data, ha ha!"};
  EXPECT_THAT(framer.SerializeFrame(data),
              EqualsFrames({{spdy::SpdyFrameType::DATA, 25}}));
}

TEST(EqualsFrames, SingleFrameWithoutLength) {
  SpdyFramer framer{SpdyFramer::ENABLE_COMPRESSION};

  spdy::SpdyRstStreamIR rst_stream{7, spdy::ERROR_CODE_REFUSED_STREAM};
  EXPECT_THAT(framer.SerializeFrame(rst_stream),
              EqualsFrames({{spdy::SpdyFrameType::RST_STREAM, std::nullopt}}));

  spdy::SpdyGoAwayIR goaway{13, spdy::ERROR_CODE_ENHANCE_YOUR_CALM,
                            "Consider taking some deep breaths."};
  EXPECT_THAT(framer.SerializeFrame(goaway),
              EqualsFrames({{spdy::SpdyFrameType::GOAWAY, std::nullopt}}));

  quiche::HttpHeaderBlock block;
  block[":method"] = "GET";
  block[":path"] = "/example";
  block[":authority"] = "example.com";
  spdy::SpdyHeadersIR headers{17, std::move(block)};
  EXPECT_THAT(framer.SerializeFrame(headers),
              EqualsFrames({{spdy::SpdyFrameType::HEADERS, std::nullopt}}));
}

TEST(EqualsFrames, MultipleFrames) {
  SpdyFramer framer{SpdyFramer::ENABLE_COMPRESSION};

  spdy::SpdyPingIR ping{511};
  spdy::SpdyWindowUpdateIR window_update{1, 101};
  spdy::SpdyDataIR data{3, "Some example data, ha ha!"};
  spdy::SpdyRstStreamIR rst_stream{7, spdy::ERROR_CODE_REFUSED_STREAM};
  spdy::SpdyGoAwayIR goaway{13, spdy::ERROR_CODE_ENHANCE_YOUR_CALM,
                            "Consider taking some deep breaths."};
  quiche::HttpHeaderBlock block;
  block[":method"] = "GET";
  block[":path"] = "/example";
  block[":authority"] = "example.com";
  spdy::SpdyHeadersIR headers{17, std::move(block)};

  const std::string frame_sequence =
      absl::StrCat(absl::string_view(framer.SerializeFrame(ping)),
                   absl::string_view(framer.SerializeFrame(window_update)),
                   absl::string_view(framer.SerializeFrame(data)),
                   absl::string_view(framer.SerializeFrame(rst_stream)),
                   absl::string_view(framer.SerializeFrame(goaway)),
                   absl::string_view(framer.SerializeFrame(headers)));
  absl::string_view frame_sequence_view = frame_sequence;
  EXPECT_THAT(frame_sequence,
              EqualsFrames({{spdy::SpdyFrameType::PING, std::nullopt},
                            {spdy::SpdyFrameType::WINDOW_UPDATE, std::nullopt},
                            {spdy::SpdyFrameType::DATA, 25},
                            {spdy::SpdyFrameType::RST_STREAM, std::nullopt},
                            {spdy::SpdyFrameType::GOAWAY, 42},
                            {spdy::SpdyFrameType::HEADERS, 19}}));
  EXPECT_THAT(frame_sequence_view,
              EqualsFrames({{spdy::SpdyFrameType::PING, std::nullopt},
                            {spdy::SpdyFrameType::WINDOW_UPDATE, std::nullopt},
                            {spdy::SpdyFrameType::DATA, 25},
                            {spdy::SpdyFrameType::RST_STREAM, std::nullopt},
                            {spdy::SpdyFrameType::GOAWAY, 42},
                            {spdy::SpdyFrameType::HEADERS, 19}}));
  EXPECT_THAT(
      frame_sequence,
      EqualsFrames(
          {spdy::SpdyFrameType::PING, spdy::SpdyFrameType::WINDOW_UPDATE,
           spdy::SpdyFrameType::DATA, spdy::SpdyFrameType::RST_STREAM,
           spdy::SpdyFrameType::GOAWAY, spdy::SpdyFrameType::HEADERS}));
  EXPECT_THAT(
      frame_sequence_view,
      EqualsFrames(
          {spdy::SpdyFrameType::PING, spdy::SpdyFrameType::WINDOW_UPDATE,
           spdy::SpdyFrameType::DATA, spdy::SpdyFrameType::RST_STREAM,
           spdy::SpdyFrameType::GOAWAY, spdy::SpdyFrameType::HEADERS}));

  // If the final frame type is removed the expectation fails, as there are
  // bytes left to read.
  EXPECT_THAT(
      frame_sequence,
      testing::Not(EqualsFrames(
          {spdy::SpdyFrameType::PING, spdy::SpdyFrameType::WINDOW_UPDATE,
           spdy::SpdyFrameType::DATA, spdy::SpdyFrameType::RST_STREAM,
           spdy::SpdyFrameType::GOAWAY})));
  EXPECT_THAT(
      frame_sequence_view,
      testing::Not(EqualsFrames(
          {spdy::SpdyFrameType::PING, spdy::SpdyFrameType::WINDOW_UPDATE,
           spdy::SpdyFrameType::DATA, spdy::SpdyFrameType::RST_STREAM,
           spdy::SpdyFrameType::GOAWAY})));
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
