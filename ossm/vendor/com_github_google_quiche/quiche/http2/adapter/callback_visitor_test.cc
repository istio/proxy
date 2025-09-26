#include "quiche/http2/adapter/callback_visitor.h"

#include <string>

#include "absl/container/flat_hash_map.h"
#include "quiche/http2/adapter/http2_protocol.h"
#include "quiche/http2/adapter/mock_nghttp2_callbacks.h"
#include "quiche/http2/adapter/nghttp2_adapter.h"
#include "quiche/http2/adapter/nghttp2_test_utils.h"
#include "quiche/http2/adapter/test_frame_sequence.h"
#include "quiche/http2/adapter/test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

using testing::_;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

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
  CONTINUATION,
};

// Tests connection-level events.
TEST(ClientCallbackVisitorUnitTest, ConnectionFrames) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  testing::InSequence seq;

  // SETTINGS
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, SETTINGS, _)));
  visitor.OnFrameHeader(0, 0, SETTINGS, 0);

  visitor.OnSettingsStart();
  EXPECT_CALL(callbacks, OnFrameRecv(IsSettings(testing::IsEmpty())));
  visitor.OnSettingsEnd();

  // PING
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, PING, _)));
  visitor.OnFrameHeader(0, 8, PING, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsPing(42)));
  visitor.OnPing(42, false);

  // WINDOW_UPDATE
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, WINDOW_UPDATE, _)));
  visitor.OnFrameHeader(0, 4, WINDOW_UPDATE, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsWindowUpdate(1000)));
  visitor.OnWindowUpdate(0, 1000);

  // PING ack
  EXPECT_CALL(callbacks,
              OnBeginFrame(HasFrameHeader(0, PING, NGHTTP2_FLAG_ACK)));
  visitor.OnFrameHeader(0, 8, PING, 1);

  EXPECT_CALL(callbacks, OnFrameRecv(IsPingAck(247)));
  visitor.OnPing(247, true);

  // GOAWAY
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, GOAWAY, 0)));
  visitor.OnFrameHeader(0, 19, GOAWAY, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsGoAway(5, NGHTTP2_ENHANCE_YOUR_CALM,
                                              "calm down!!")));
  visitor.OnGoAway(5, Http2ErrorCode::ENHANCE_YOUR_CALM, "calm down!!");

  EXPECT_EQ(visitor.stream_map_size(), 0);
}

TEST(ClientCallbackVisitorUnitTest, StreamFrames) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);
  absl::flat_hash_map<Http2StreamId, int> stream_close_counts;
  visitor.set_stream_close_listener(
      [&stream_close_counts](Http2StreamId stream_id) {
        ++stream_close_counts[stream_id];
      });

  testing::InSequence seq;

  EXPECT_EQ(visitor.stream_map_size(), 0);

  // HEADERS on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, HEADERS, _)));
  visitor.OnFrameHeader(1, 23, HEADERS, 4);

  EXPECT_CALL(callbacks,
              OnBeginHeaders(IsHeaders(1, _, NGHTTP2_HCAT_RESPONSE)));
  visitor.OnBeginHeadersForStream(1);

  EXPECT_EQ(visitor.stream_map_size(), 1);

  EXPECT_CALL(callbacks, OnHeader(_, ":status", "200", _));
  visitor.OnHeaderForStream(1, ":status", "200");

  EXPECT_CALL(callbacks, OnHeader(_, "server", "my-fake-server", _));
  visitor.OnHeaderForStream(1, "server", "my-fake-server");

  EXPECT_CALL(callbacks,
              OnHeader(_, "date", "Tue, 6 Apr 2021 12:54:01 GMT", _));
  visitor.OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT");

  EXPECT_CALL(callbacks, OnHeader(_, "trailer", "x-server-status", _));
  visitor.OnHeaderForStream(1, "trailer", "x-server-status");

  EXPECT_CALL(callbacks, OnFrameRecv(IsHeaders(1, _, NGHTTP2_HCAT_RESPONSE)));
  visitor.OnEndHeadersForStream(1);

  // DATA for stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, DATA, 0)));
  visitor.OnFrameHeader(1, 26, DATA, 0);

  visitor.OnBeginDataForStream(1, 26);
  EXPECT_CALL(callbacks, OnDataChunkRecv(0, 1, "This is the response body."));
  EXPECT_CALL(callbacks, OnFrameRecv(IsData(1, _, 0)));
  visitor.OnDataForStream(1, "This is the response body.");

  // Trailers for stream 1, with a different nghttp2 "category".
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, HEADERS, _)));
  visitor.OnFrameHeader(1, 23, HEADERS, 4);

  EXPECT_CALL(callbacks, OnBeginHeaders(IsHeaders(1, _, NGHTTP2_HCAT_HEADERS)));
  visitor.OnBeginHeadersForStream(1);

  EXPECT_CALL(callbacks, OnHeader(_, "x-server-status", "OK", _));
  visitor.OnHeaderForStream(1, "x-server-status", "OK");

  EXPECT_CALL(callbacks, OnFrameRecv(IsHeaders(1, _, NGHTTP2_HCAT_HEADERS)));
  visitor.OnEndHeadersForStream(1);

  EXPECT_THAT(stream_close_counts, IsEmpty());

  // RST_STREAM on stream 3
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(3, RST_STREAM, 0)));
  visitor.OnFrameHeader(3, 4, RST_STREAM, 0);

  // No change in stream map size.
  EXPECT_EQ(visitor.stream_map_size(), 1);
  EXPECT_THAT(stream_close_counts, IsEmpty());

  EXPECT_CALL(callbacks, OnFrameRecv(IsRstStream(3, NGHTTP2_INTERNAL_ERROR)));
  visitor.OnRstStream(3, Http2ErrorCode::INTERNAL_ERROR);

  EXPECT_CALL(callbacks, OnStreamClose(3, NGHTTP2_INTERNAL_ERROR));
  visitor.OnCloseStream(3, Http2ErrorCode::INTERNAL_ERROR);

  EXPECT_THAT(stream_close_counts, UnorderedElementsAre(Pair(3, 1)));

  // More stream close events
  EXPECT_CALL(callbacks,
              OnBeginFrame(HasFrameHeader(1, DATA, NGHTTP2_FLAG_END_STREAM)));
  visitor.OnFrameHeader(1, 0, DATA, 1);

  EXPECT_CALL(callbacks, OnFrameRecv(IsData(1, _, NGHTTP2_FLAG_END_STREAM)));
  visitor.OnBeginDataForStream(1, 0);
  EXPECT_TRUE(visitor.OnEndStream(1));

  EXPECT_CALL(callbacks, OnStreamClose(1, NGHTTP2_NO_ERROR));
  visitor.OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR);

  // Stream map is empty again after both streams were closed.
  EXPECT_EQ(visitor.stream_map_size(), 0);
  EXPECT_THAT(stream_close_counts,
              UnorderedElementsAre(Pair(3, 1), Pair(1, 1)));

  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(5, RST_STREAM, _)));
  visitor.OnFrameHeader(5, 4, RST_STREAM, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsRstStream(5, NGHTTP2_REFUSED_STREAM)));
  visitor.OnRstStream(5, Http2ErrorCode::REFUSED_STREAM);

  EXPECT_CALL(callbacks, OnStreamClose(5, NGHTTP2_REFUSED_STREAM));
  visitor.OnCloseStream(5, Http2ErrorCode::REFUSED_STREAM);

  EXPECT_EQ(visitor.stream_map_size(), 0);
  EXPECT_THAT(stream_close_counts,
              UnorderedElementsAre(Pair(3, 1), Pair(1, 1), Pair(5, 1)));
}

TEST(ClientCallbackVisitorUnitTest, HeadersWithContinuation) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  testing::InSequence seq;

  // HEADERS on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, HEADERS, 0x0)));
  ASSERT_TRUE(visitor.OnFrameHeader(1, 23, HEADERS, 0x0));

  EXPECT_CALL(callbacks,
              OnBeginHeaders(IsHeaders(1, _, NGHTTP2_HCAT_RESPONSE)));
  visitor.OnBeginHeadersForStream(1);

  EXPECT_CALL(callbacks, OnHeader(_, ":status", "200", _));
  visitor.OnHeaderForStream(1, ":status", "200");

  EXPECT_CALL(callbacks, OnHeader(_, "server", "my-fake-server", _));
  visitor.OnHeaderForStream(1, "server", "my-fake-server");

  EXPECT_CALL(callbacks,
              OnBeginFrame(HasFrameHeader(1, CONTINUATION, END_HEADERS_FLAG)));
  ASSERT_TRUE(visitor.OnFrameHeader(1, 23, CONTINUATION, END_HEADERS_FLAG));

  EXPECT_CALL(callbacks,
              OnHeader(_, "date", "Tue, 6 Apr 2021 12:54:01 GMT", _));
  visitor.OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT");

  EXPECT_CALL(callbacks, OnHeader(_, "trailer", "x-server-status", _));
  visitor.OnHeaderForStream(1, "trailer", "x-server-status");

  EXPECT_CALL(callbacks, OnFrameRecv(IsHeaders(1, _, NGHTTP2_HCAT_RESPONSE)));
  visitor.OnEndHeadersForStream(1);
}

TEST(ClientCallbackVisitorUnitTest, ContinuationNoHeaders) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);
  // Because no stream precedes the CONTINUATION frame, the stream ID does not
  // match, and the method returns false.
  EXPECT_FALSE(visitor.OnFrameHeader(1, 23, CONTINUATION, END_HEADERS_FLAG));
}

TEST(ClientCallbackVisitorUnitTest, ContinuationWrongPrecedingType) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, WINDOW_UPDATE, _)));
  visitor.OnFrameHeader(1, 4, WINDOW_UPDATE, 0);

  // Because the CONTINUATION frame does not follow HEADERS, the method returns
  // false.
  EXPECT_FALSE(visitor.OnFrameHeader(1, 23, CONTINUATION, END_HEADERS_FLAG));
}

TEST(ClientCallbackVisitorUnitTest, ContinuationWrongStream) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);
  // HEADERS on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, HEADERS, 0x0)));
  ASSERT_TRUE(visitor.OnFrameHeader(1, 23, HEADERS, 0x0));

  EXPECT_CALL(callbacks,
              OnBeginHeaders(IsHeaders(1, _, NGHTTP2_HCAT_RESPONSE)));
  visitor.OnBeginHeadersForStream(1);

  EXPECT_CALL(callbacks, OnHeader(_, ":status", "200", _));
  visitor.OnHeaderForStream(1, ":status", "200");

  EXPECT_CALL(callbacks, OnHeader(_, "server", "my-fake-server", _));
  visitor.OnHeaderForStream(1, "server", "my-fake-server");

  // The CONTINUATION stream ID does not match the one from the HEADERS.
  EXPECT_FALSE(visitor.OnFrameHeader(3, 23, CONTINUATION, END_HEADERS_FLAG));
}

TEST(ClientCallbackVisitorUnitTest, ResetAndGoaway) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kClient,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  testing::InSequence seq;

  // RST_STREAM on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, RST_STREAM, 0x0)));
  EXPECT_TRUE(visitor.OnFrameHeader(1, 13, RST_STREAM, 0x0));

  EXPECT_CALL(callbacks, OnFrameRecv(IsRstStream(1, NGHTTP2_INTERNAL_ERROR)));
  visitor.OnRstStream(1, Http2ErrorCode::INTERNAL_ERROR);

  EXPECT_CALL(callbacks, OnStreamClose(1, NGHTTP2_INTERNAL_ERROR));
  EXPECT_TRUE(visitor.OnCloseStream(1, Http2ErrorCode::INTERNAL_ERROR));

  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, GOAWAY, 0x0)));
  EXPECT_TRUE(visitor.OnFrameHeader(0, 13, GOAWAY, 0x0));

  EXPECT_CALL(callbacks,
              OnFrameRecv(IsGoAway(3, NGHTTP2_ENHANCE_YOUR_CALM, "calma te")));
  EXPECT_TRUE(
      visitor.OnGoAway(3, Http2ErrorCode::ENHANCE_YOUR_CALM, "calma te"));

  EXPECT_CALL(callbacks, OnStreamClose(5, NGHTTP2_STREAM_CLOSED))
      .WillOnce(testing::Return(NGHTTP2_ERR_CALLBACK_FAILURE));
  EXPECT_FALSE(visitor.OnCloseStream(5, Http2ErrorCode::STREAM_CLOSED));
}

TEST(ServerCallbackVisitorUnitTest, ConnectionFrames) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kServer,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  testing::InSequence seq;

  // SETTINGS
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, SETTINGS, _)));
  visitor.OnFrameHeader(0, 0, SETTINGS, 0);

  visitor.OnSettingsStart();
  EXPECT_CALL(callbacks, OnFrameRecv(IsSettings(testing::IsEmpty())));
  visitor.OnSettingsEnd();

  // PING
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, PING, _)));
  visitor.OnFrameHeader(0, 8, PING, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsPing(42)));
  visitor.OnPing(42, false);

  // WINDOW_UPDATE
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, WINDOW_UPDATE, _)));
  visitor.OnFrameHeader(0, 4, WINDOW_UPDATE, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsWindowUpdate(1000)));
  visitor.OnWindowUpdate(0, 1000);

  // PING ack
  EXPECT_CALL(callbacks,
              OnBeginFrame(HasFrameHeader(0, PING, NGHTTP2_FLAG_ACK)));
  visitor.OnFrameHeader(0, 8, PING, 1);

  EXPECT_CALL(callbacks, OnFrameRecv(IsPingAck(247)));
  visitor.OnPing(247, true);

  EXPECT_EQ(visitor.stream_map_size(), 0);
}

TEST(ServerCallbackVisitorUnitTest, StreamFrames) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kServer,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  testing::InSequence seq;

  // HEADERS on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(
                             1, HEADERS, NGHTTP2_FLAG_END_HEADERS)));
  visitor.OnFrameHeader(1, 23, HEADERS, 4);

  EXPECT_CALL(callbacks, OnBeginHeaders(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                                  NGHTTP2_HCAT_REQUEST)));
  visitor.OnBeginHeadersForStream(1);

  EXPECT_EQ(visitor.stream_map_size(), 1);

  EXPECT_CALL(callbacks, OnHeader(_, ":method", "POST", _));
  visitor.OnHeaderForStream(1, ":method", "POST");

  EXPECT_CALL(callbacks, OnHeader(_, ":path", "/example/path", _));
  visitor.OnHeaderForStream(1, ":path", "/example/path");

  EXPECT_CALL(callbacks, OnHeader(_, ":scheme", "https", _));
  visitor.OnHeaderForStream(1, ":scheme", "https");

  EXPECT_CALL(callbacks, OnHeader(_, ":authority", "example.com", _));
  visitor.OnHeaderForStream(1, ":authority", "example.com");

  EXPECT_CALL(callbacks, OnHeader(_, "accept", "text/html", _));
  visitor.OnHeaderForStream(1, "accept", "text/html");

  EXPECT_CALL(callbacks, OnFrameRecv(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                               NGHTTP2_HCAT_REQUEST)));
  visitor.OnEndHeadersForStream(1);

  // DATA on stream 1
  EXPECT_CALL(callbacks,
              OnBeginFrame(HasFrameHeader(1, DATA, NGHTTP2_FLAG_END_STREAM)));
  visitor.OnFrameHeader(1, 25, DATA, NGHTTP2_FLAG_END_STREAM);

  visitor.OnBeginDataForStream(1, 25);
  EXPECT_CALL(callbacks, OnDataChunkRecv(NGHTTP2_FLAG_END_STREAM, 1,
                                         "This is the request body."));
  EXPECT_CALL(callbacks, OnFrameRecv(IsData(1, _, NGHTTP2_FLAG_END_STREAM)));
  visitor.OnDataForStream(1, "This is the request body.");
  EXPECT_TRUE(visitor.OnEndStream(1));

  EXPECT_CALL(callbacks, OnStreamClose(1, NGHTTP2_NO_ERROR));
  visitor.OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR);

  EXPECT_EQ(visitor.stream_map_size(), 0);

  // RST_STREAM on stream 3
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(3, RST_STREAM, 0)));
  visitor.OnFrameHeader(3, 4, RST_STREAM, 0);

  EXPECT_CALL(callbacks, OnFrameRecv(IsRstStream(3, NGHTTP2_INTERNAL_ERROR)));
  visitor.OnRstStream(3, Http2ErrorCode::INTERNAL_ERROR);

  EXPECT_CALL(callbacks, OnStreamClose(3, NGHTTP2_INTERNAL_ERROR));
  visitor.OnCloseStream(3, Http2ErrorCode::INTERNAL_ERROR);

  EXPECT_EQ(visitor.stream_map_size(), 0);
}

TEST(ServerCallbackVisitorUnitTest, DataWithPadding) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kServer,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  const size_t kPaddingLength = 39;
  const uint8_t kFlags = NGHTTP2_FLAG_PADDED | NGHTTP2_FLAG_END_STREAM;

  testing::InSequence seq;

  // DATA on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(1, DATA, kFlags)));
  EXPECT_TRUE(visitor.OnFrameHeader(1, 25 + kPaddingLength, DATA, kFlags));

  EXPECT_TRUE(visitor.OnBeginDataForStream(1, 25 + kPaddingLength));

  // Padding before data.
  EXPECT_TRUE(visitor.OnDataPaddingLength(1, kPaddingLength));

  EXPECT_CALL(callbacks,
              OnDataChunkRecv(kFlags, 1, "This is the request body."));
  EXPECT_CALL(callbacks, OnFrameRecv(IsData(1, _, kFlags, kPaddingLength)));
  EXPECT_TRUE(visitor.OnDataForStream(1, "This is the request body."));
  EXPECT_TRUE(visitor.OnEndStream(1));

  EXPECT_CALL(callbacks, OnStreamClose(1, NGHTTP2_NO_ERROR));
  visitor.OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR);

  // DATA on stream 3
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(3, DATA, kFlags)));
  EXPECT_TRUE(visitor.OnFrameHeader(3, 25 + kPaddingLength, DATA, kFlags));

  EXPECT_TRUE(visitor.OnBeginDataForStream(3, 25 + kPaddingLength));

  // Data before padding.
  EXPECT_CALL(callbacks,
              OnDataChunkRecv(kFlags, 3, "This is the request body."));
  EXPECT_TRUE(visitor.OnDataForStream(3, "This is the request body."));

  EXPECT_CALL(callbacks, OnFrameRecv(IsData(3, _, kFlags, kPaddingLength)));
  EXPECT_TRUE(visitor.OnDataPaddingLength(3, kPaddingLength));
  EXPECT_TRUE(visitor.OnEndStream(3));

  EXPECT_CALL(callbacks, OnStreamClose(3, NGHTTP2_NO_ERROR));
  visitor.OnCloseStream(3, Http2ErrorCode::HTTP2_NO_ERROR);

  // DATA on stream 5
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(5, DATA, kFlags)));
  EXPECT_TRUE(visitor.OnFrameHeader(5, 25 + kPaddingLength, DATA, kFlags));

  EXPECT_TRUE(visitor.OnBeginDataForStream(5, 25 + kPaddingLength));

  // Error during padding.
  EXPECT_CALL(callbacks,
              OnDataChunkRecv(kFlags, 5, "This is the request body."));
  EXPECT_TRUE(visitor.OnDataForStream(5, "This is the request body."));

  EXPECT_CALL(callbacks, OnFrameRecv(IsData(5, _, kFlags, kPaddingLength)))
      .WillOnce(testing::Return(NGHTTP2_ERR_CALLBACK_FAILURE));
  EXPECT_TRUE(visitor.OnDataPaddingLength(5, kPaddingLength));
  EXPECT_FALSE(visitor.OnEndStream(3));

  EXPECT_CALL(callbacks, OnStreamClose(5, NGHTTP2_NO_ERROR));
  visitor.OnCloseStream(5, Http2ErrorCode::HTTP2_NO_ERROR);
}

// In the case of a Content-Length mismatch where the header value is larger
// than the actual data for the stream, nghttp2 will call
// `on_begin_frame_callback` and `on_data_chunk_recv_callback`, but not the
// `on_frame_recv_callback`.
TEST(ServerCallbackVisitorUnitTest, MismatchedContentLengthCallbacks) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kServer,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/"},
                                           {"content-length", "50"}},
                                          /*fin=*/false)
                                 .Data(1, "Less than 50 bytes.", true)
                                 .Serialize();

  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(0, SETTINGS, _)));

  EXPECT_CALL(callbacks, OnFrameRecv(IsSettings(testing::IsEmpty())));

  // HEADERS on stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(
                             1, HEADERS, NGHTTP2_FLAG_END_HEADERS)));

  EXPECT_CALL(callbacks, OnBeginHeaders(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                                  NGHTTP2_HCAT_REQUEST)));

  EXPECT_CALL(callbacks, OnHeader(_, ":method", "POST", _));
  EXPECT_CALL(callbacks, OnHeader(_, ":path", "/", _));
  EXPECT_CALL(callbacks, OnHeader(_, ":scheme", "https", _));
  EXPECT_CALL(callbacks, OnHeader(_, ":authority", "example.com", _));
  EXPECT_CALL(callbacks, OnHeader(_, "content-length", "50", _));
  EXPECT_CALL(callbacks, OnFrameRecv(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                               NGHTTP2_HCAT_REQUEST)));

  // DATA on stream 1
  EXPECT_CALL(callbacks,
              OnBeginFrame(HasFrameHeader(1, DATA, NGHTTP2_FLAG_END_STREAM)));

  EXPECT_CALL(callbacks, OnDataChunkRecv(NGHTTP2_FLAG_END_STREAM, 1,
                                         "Less than 50 bytes."));

  // Like nghttp2, CallbackVisitor does not pass on a call to OnFrameRecv in the
  // case of Content-Length mismatch.

  int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);
}

TEST(ServerCallbackVisitorUnitTest, HeadersAfterFin) {
  testing::StrictMock<MockNghttp2Callbacks> callbacks;
  CallbackVisitor visitor(Perspective::kServer,
                          *MockNghttp2Callbacks::GetCallbacks(), &callbacks);

  testing::InSequence seq;

  // HEADERS on stream 1
  EXPECT_CALL(
      callbacks,
      OnBeginFrame(HasFrameHeader(
          1, HEADERS, NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM)));
  visitor.OnFrameHeader(1, 23, HEADERS, 5);

  EXPECT_CALL(callbacks,
              OnBeginHeaders(IsHeaders(
                  1, NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                  NGHTTP2_HCAT_REQUEST)));
  EXPECT_TRUE(visitor.OnBeginHeadersForStream(1));

  EXPECT_EQ(visitor.stream_map_size(), 1);

  EXPECT_CALL(callbacks, OnHeader).Times(5);
  visitor.OnHeaderForStream(1, ":method", "POST");
  visitor.OnHeaderForStream(1, ":path", "/example/path");
  visitor.OnHeaderForStream(1, ":scheme", "https");
  visitor.OnHeaderForStream(1, ":authority", "example.com");
  visitor.OnHeaderForStream(1, "accept", "text/html");

  EXPECT_CALL(callbacks,
              OnFrameRecv(IsHeaders(
                  1, NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM,
                  NGHTTP2_HCAT_REQUEST)));
  visitor.OnEndHeadersForStream(1);

  EXPECT_TRUE(visitor.OnEndStream(1));

  EXPECT_CALL(callbacks, OnStreamClose(1, NGHTTP2_NO_ERROR));
  visitor.OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR);

  EXPECT_EQ(visitor.stream_map_size(), 0);

  // Invalid repeat HEADERS on closed stream 1
  EXPECT_CALL(callbacks, OnBeginFrame(HasFrameHeader(
                             1, HEADERS, NGHTTP2_FLAG_END_HEADERS)));
  visitor.OnFrameHeader(1, 23, HEADERS, 4);

  EXPECT_CALL(callbacks, OnBeginHeaders(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                                  NGHTTP2_HCAT_HEADERS)));
  EXPECT_TRUE(visitor.OnBeginHeadersForStream(1));

  // The visitor should not revive streams that have already been closed.
  EXPECT_EQ(visitor.stream_map_size(), 0);
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
