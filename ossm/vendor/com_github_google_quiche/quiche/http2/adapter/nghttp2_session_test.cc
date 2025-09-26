#include "quiche/http2/adapter/nghttp2_session.h"

#include <string>
#include <vector>

#include "quiche/http2/adapter/mock_http2_visitor.h"
#include "quiche/http2/adapter/nghttp2_callbacks.h"
#include "quiche/http2/adapter/nghttp2_util.h"
#include "quiche/http2/adapter/test_frame_sequence.h"
#include "quiche/http2/adapter/test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

using testing::_;

enum FrameType {
  DATA,
  HEADERS,
  PRIORITY,
  RST_STREAM,
  SETTINGS,
  PUSH_PROMISE,
  PING,
  GOAWAY,
  WINDOW_UPDATE,
};

class NgHttp2SessionTest : public quiche::test::QuicheTest {
 public:
  void SetUp() override {
    nghttp2_option_new(&options_);
    nghttp2_option_set_no_auto_window_update(options_, 1);
  }

  void TearDown() override { nghttp2_option_del(options_); }

  nghttp2_session_callbacks_unique_ptr CreateCallbacks() {
    nghttp2_session_callbacks_unique_ptr callbacks = callbacks::Create(nullptr);
    return callbacks;
  }

  TestVisitor visitor_;
  nghttp2_option* options_ = nullptr;
};

TEST_F(NgHttp2SessionTest, ClientConstruction) {
  NgHttp2Session session(Perspective::kClient, CreateCallbacks(), options_,
                         &visitor_);
  EXPECT_TRUE(session.want_read());
  EXPECT_FALSE(session.want_write());
  EXPECT_EQ(session.GetRemoteWindowSize(), kInitialFlowControlWindowSize);
  EXPECT_NE(session.raw_ptr(), nullptr);
}

TEST_F(NgHttp2SessionTest, ClientHandlesFrames) {
  NgHttp2Session session(Perspective::kClient, CreateCallbacks(), options_,
                         &visitor_);

  ASSERT_EQ(0, nghttp2_session_send(session.raw_ptr()));
  ASSERT_GT(visitor_.data().size(), 0);

  const std::string initial_frames = TestFrameSequence()
                                         .ServerPreface()
                                         .Ping(42)
                                         .WindowUpdate(0, 1000)
                                         .Serialize();
  testing::InSequence s;

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor_, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor_, OnSettingsStart());
  EXPECT_CALL(visitor_, OnSettingsEnd());

  EXPECT_CALL(visitor_, OnFrameHeader(0, 8, PING, 0));
  EXPECT_CALL(visitor_, OnPing(42, false));
  EXPECT_CALL(visitor_, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor_, OnWindowUpdate(0, 1000));

  const int64_t initial_result = session.ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_EQ(session.GetRemoteWindowSize(),
            kInitialFlowControlWindowSize + 1000);

  EXPECT_CALL(visitor_, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor_, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor_, OnBeforeFrameSent(PING, 0, 8, 0x1));
  EXPECT_CALL(visitor_, OnFrameSent(PING, 0, 8, 0x1, 0));

  ASSERT_EQ(0, nghttp2_session_send(session.raw_ptr()));
  // Some bytes should have been serialized.
  absl::string_view serialized = visitor_.data();
  ASSERT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized, EqualsFrames({spdy::SpdyFrameType::SETTINGS,
                                        spdy::SpdyFrameType::PING}));
  visitor_.Clear();

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const auto nvs1 = GetNghttp2Nvs(headers1);

  const std::vector<Header> headers2 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/two"}});
  const auto nvs2 = GetNghttp2Nvs(headers2);

  const std::vector<Header> headers3 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/three"}});
  const auto nvs3 = GetNghttp2Nvs(headers3);

  const int32_t stream_id1 = nghttp2_submit_request(
      session.raw_ptr(), nullptr, nvs1.data(), nvs1.size(), nullptr, nullptr);
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  const int32_t stream_id2 = nghttp2_submit_request(
      session.raw_ptr(), nullptr, nvs2.data(), nvs2.size(), nullptr, nullptr);
  ASSERT_GT(stream_id2, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id2;

  const int32_t stream_id3 = nghttp2_submit_request(
      session.raw_ptr(), nullptr, nvs3.data(), nvs3.size(), nullptr, nullptr);
  ASSERT_GT(stream_id3, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id3;

  EXPECT_CALL(visitor_, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor_, OnFrameSent(HEADERS, 1, _, 0x5, 0));
  EXPECT_CALL(visitor_, OnBeforeFrameSent(HEADERS, 3, _, 0x5));
  EXPECT_CALL(visitor_, OnFrameSent(HEADERS, 3, _, 0x5, 0));
  EXPECT_CALL(visitor_, OnBeforeFrameSent(HEADERS, 5, _, 0x5));
  EXPECT_CALL(visitor_, OnFrameSent(HEADERS, 5, _, 0x5, 0));

  ASSERT_EQ(0, nghttp2_session_send(session.raw_ptr()));
  serialized = visitor_.data();
  EXPECT_THAT(serialized, EqualsFrames({spdy::SpdyFrameType::HEADERS,
                                        spdy::SpdyFrameType::HEADERS,
                                        spdy::SpdyFrameType::HEADERS}));
  visitor_.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.")
          .RstStream(3, Http2ErrorCode::INTERNAL_ERROR)
          .GoAway(5, Http2ErrorCode::ENHANCE_YOUR_CALM, "calm down!!")
          .Serialize();

  EXPECT_CALL(visitor_, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor_, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor_,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor_, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor_, OnFrameHeader(1, 26, DATA, 0));
  EXPECT_CALL(visitor_, OnBeginDataForStream(1, 26));
  EXPECT_CALL(visitor_, OnDataForStream(1, "This is the response body."));
  EXPECT_CALL(visitor_, OnFrameHeader(3, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor_, OnRstStream(3, Http2ErrorCode::INTERNAL_ERROR));
  EXPECT_CALL(visitor_, OnCloseStream(3, Http2ErrorCode::INTERNAL_ERROR));
  EXPECT_CALL(visitor_, OnFrameHeader(0, 19, GOAWAY, 0));
  EXPECT_CALL(visitor_,
              OnGoAway(5, Http2ErrorCode::ENHANCE_YOUR_CALM, "calm down!!"));
  const int64_t stream_result = session.ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  // Even though the client recieved a GOAWAY, streams 1 and 5 are still active.
  EXPECT_TRUE(session.want_read());

  EXPECT_CALL(visitor_, OnFrameHeader(1, 0, DATA, 1));
  EXPECT_CALL(visitor_, OnBeginDataForStream(1, 0));
  EXPECT_CALL(visitor_, OnEndStream(1));
  EXPECT_CALL(visitor_, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));
  EXPECT_CALL(visitor_, OnFrameHeader(5, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor_, OnRstStream(5, Http2ErrorCode::REFUSED_STREAM));
  EXPECT_CALL(visitor_, OnCloseStream(5, Http2ErrorCode::REFUSED_STREAM));
  session.ProcessBytes(TestFrameSequence()
                           .Data(1, "", true)
                           .RstStream(5, Http2ErrorCode::REFUSED_STREAM)
                           .Serialize());
  // After receiving END_STREAM for 1 and RST_STREAM for 5, the session no
  // longer expects reads.
  EXPECT_FALSE(session.want_read());

  // Client will not have anything else to write.
  EXPECT_FALSE(session.want_write());
  ASSERT_EQ(0, nghttp2_session_send(session.raw_ptr()));
  serialized = visitor_.data();
  EXPECT_EQ(serialized.size(), 0);
}

TEST_F(NgHttp2SessionTest, ServerConstruction) {
  NgHttp2Session session(Perspective::kServer, CreateCallbacks(), options_,
                         &visitor_);
  EXPECT_TRUE(session.want_read());
  EXPECT_FALSE(session.want_write());
  EXPECT_EQ(session.GetRemoteWindowSize(), kInitialFlowControlWindowSize);
  EXPECT_NE(session.raw_ptr(), nullptr);
}

TEST_F(NgHttp2SessionTest, ServerHandlesFrames) {
  NgHttp2Session session(Perspective::kServer, CreateCallbacks(), options_,
                         &visitor_);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Ping(42)
                                 .WindowUpdate(0, 1000)
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .Headers(3,
                                          {{":method", "GET"},
                                           {":scheme", "http"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/two"}},
                                          /*fin=*/true)
                                 .RstStream(3, Http2ErrorCode::CANCEL)
                                 .Ping(47)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor_, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor_, OnSettingsStart());
  EXPECT_CALL(visitor_, OnSettingsEnd());

  EXPECT_CALL(visitor_, OnFrameHeader(0, 8, PING, 0));
  EXPECT_CALL(visitor_, OnPing(42, false));
  EXPECT_CALL(visitor_, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor_, OnWindowUpdate(0, 1000));
  EXPECT_CALL(visitor_, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor_, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor_, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor_, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor_, OnWindowUpdate(1, 2000));
  EXPECT_CALL(visitor_, OnFrameHeader(1, 25, DATA, 0));
  EXPECT_CALL(visitor_, OnBeginDataForStream(1, 25));
  EXPECT_CALL(visitor_, OnDataForStream(1, "This is the request body."));
  EXPECT_CALL(visitor_, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor_, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor_, OnHeaderForStream(3, ":method", "GET"));
  EXPECT_CALL(visitor_, OnHeaderForStream(3, ":scheme", "http"));
  EXPECT_CALL(visitor_, OnHeaderForStream(3, ":authority", "example.com"));
  EXPECT_CALL(visitor_, OnHeaderForStream(3, ":path", "/this/is/request/two"));
  EXPECT_CALL(visitor_, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor_, OnEndStream(3));
  EXPECT_CALL(visitor_, OnFrameHeader(3, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor_, OnRstStream(3, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor_, OnCloseStream(3, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor_, OnFrameHeader(0, 8, PING, 0));
  EXPECT_CALL(visitor_, OnPing(47, false));

  const int64_t result = session.ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(session.GetRemoteWindowSize(),
            kInitialFlowControlWindowSize + 1000);

  EXPECT_CALL(visitor_, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor_, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor_, OnBeforeFrameSent(PING, 0, 8, 0x1));
  EXPECT_CALL(visitor_, OnFrameSent(PING, 0, 8, 0x1, 0));
  EXPECT_CALL(visitor_, OnBeforeFrameSent(PING, 0, 8, 0x1));
  EXPECT_CALL(visitor_, OnFrameSent(PING, 0, 8, 0x1, 0));

  EXPECT_TRUE(session.want_write());
  ASSERT_EQ(0, nghttp2_session_send(session.raw_ptr()));
  // Some bytes should have been serialized.
  absl::string_view serialized = visitor_.data();
  // SETTINGS ack, two PING acks.
  EXPECT_THAT(serialized, EqualsFrames({spdy::SpdyFrameType::SETTINGS,
                                        spdy::SpdyFrameType::PING,
                                        spdy::SpdyFrameType::PING}));
}

// Verifies that a null payload is caught by the OnPackExtensionCallback
// implementation.
TEST_F(NgHttp2SessionTest, NullPayload) {
  NgHttp2Session session(Perspective::kClient, CreateCallbacks(), options_,
                         &visitor_);

  void* payload = nullptr;
  const int result = nghttp2_submit_extension(
      session.raw_ptr(), kMetadataFrameType, 0, 1, payload);
  ASSERT_EQ(0, result);
  EXPECT_TRUE(session.want_write());
  int send_result = -1;
  EXPECT_QUICHE_BUG(
      {
        send_result = nghttp2_session_send(session.raw_ptr());
        EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, send_result);
      },
      "Extension frame payload for stream 1 is null!");
}

TEST_F(NgHttp2SessionTest, ServerSeesErrorOnEndStream) {
  NgHttp2Session session(Perspective::kServer, CreateCallbacks(), options_,
                         &visitor_);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/"}},
                                          /*fin=*/false)
                                 .Data(1, "Request body", true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor_, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor_, OnSettingsStart());
  EXPECT_CALL(visitor_, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor_, OnFrameHeader(1, _, HEADERS, 0x4));
  EXPECT_CALL(visitor_, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor_, OnHeaderForStream(1, ":path", "/"));
  EXPECT_CALL(visitor_, OnEndHeadersForStream(1));

  EXPECT_CALL(visitor_, OnFrameHeader(1, _, DATA, 0x1));
  EXPECT_CALL(visitor_, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor_, OnDataForStream(1, "Request body"));
  EXPECT_CALL(visitor_, OnEndStream(1)).WillOnce(testing::Return(false));

  const int64_t result = session.ProcessBytes(frames);
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, result);

  EXPECT_TRUE(session.want_write());

  EXPECT_CALL(visitor_, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor_, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  ASSERT_EQ(0, nghttp2_session_send(session.raw_ptr()));
  EXPECT_THAT(visitor_.data(), EqualsFrames({spdy::SpdyFrameType::SETTINGS}));
  visitor_.Clear();

  EXPECT_FALSE(session.want_write());
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
