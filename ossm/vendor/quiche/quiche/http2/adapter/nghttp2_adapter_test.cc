#include "quiche/http2/adapter/nghttp2_adapter.h"

#include <memory>
#include <string>
#include <vector>

#include "quiche/http2/adapter/http2_protocol.h"
#include "quiche/http2/adapter/http2_visitor_interface.h"
#include "quiche/http2/adapter/mock_http2_visitor.h"
#include "quiche/http2/adapter/nghttp2.h"
#include "quiche/http2/adapter/nghttp2_test_utils.h"
#include "quiche/http2/adapter/oghttp2_util.h"
#include "quiche/http2/adapter/test_frame_sequence.h"
#include "quiche/http2/adapter/test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

using ConnectionError = Http2VisitorInterface::ConnectionError;
using OnHeaderResult = ::http2::adapter::Http2VisitorInterface::OnHeaderResult;

using spdy::SpdyFrameType;
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
  CONTINUATION,
};

TEST(NgHttp2AdapterTest, ClientConstruction) {
  testing::StrictMock<MockHttp2Visitor> visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  ASSERT_NE(nullptr, adapter);
  EXPECT_TRUE(adapter->want_read());
  EXPECT_FALSE(adapter->want_write());
  EXPECT_FALSE(adapter->IsServerSession());
}

TEST(NgHttp2AdapterTest, ClientHandlesFrames) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              testing::StrEq(spdy::kHttp2ConnectionHeaderPrefix));
  visitor.Clear();

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  const std::string initial_frames = TestFrameSequence()
                                         .ServerPreface()
                                         .Ping(42)
                                         .WindowUpdate(0, 1000)
                                         .Serialize();
  testing::InSequence s;

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(0, 8, PING, 0));
  EXPECT_CALL(visitor, OnPing(42, false));
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 1000));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_EQ(adapter->GetSendWindowSize(), kInitialFlowControlWindowSize + 1000);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(PING, 0, 8, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(PING, 0, 8, 0x1, 0));

  result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::PING}));
  visitor.Clear();

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const std::vector<Header> headers2 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/two"}});

  const std::vector<Header> headers3 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/three"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const char* kSentinel3 = "arbitrary pointer 3";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  const int32_t stream_id2 = adapter->SubmitRequest(headers2, true, nullptr);
  ASSERT_GT(stream_id2, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id2;

  const int32_t stream_id3 =
      adapter->SubmitRequest(headers3, true, const_cast<char*>(kSentinel3));
  ASSERT_GT(stream_id3, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id3;

  const char* kSentinel2 = "arbitrary pointer 2";
  adapter->SetStreamUserData(stream_id2, const_cast<char*>(kSentinel2));
  adapter->SetStreamUserData(stream_id3, nullptr);

  // These requests did not include a body, so they do not have corresponding
  // DataFrameSources.
  EXPECT_EQ(adapter->sources_size(), 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id2, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id2, _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id3, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id3, _, 0x5, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::HEADERS,
                            SpdyFrameType::HEADERS}));
  visitor.Clear();

  // All streams are active and have not yet received any data, so the receive
  // window should be at the initial value.
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(stream_id1));
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(stream_id2));
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(stream_id3));

  // Upper bound on the flow control receive window should be the initial value.
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowLimit(stream_id1));

  // Connection has not yet received any data.
  EXPECT_EQ(kInitialFlowControlWindowSize, adapter->GetReceiveWindowSize());

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  EXPECT_EQ(kSentinel1, adapter->GetStreamUserData(stream_id1));
  EXPECT_EQ(kSentinel2, adapter->GetStreamUserData(stream_id2));
  EXPECT_EQ(nullptr, adapter->GetStreamUserData(stream_id3));

  EXPECT_EQ(0, adapter->GetHpackDecoderDynamicTableSize());

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

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 26, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 26));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the response body."));
  EXPECT_CALL(visitor, OnFrameHeader(3, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnRstStream(3, Http2ErrorCode::INTERNAL_ERROR));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::INTERNAL_ERROR))
      .WillOnce(
          [&adapter](Http2StreamId stream_id, Http2ErrorCode /*error_code*/) {
            adapter->RemoveStream(stream_id);
            return true;
          });
  EXPECT_CALL(visitor, OnFrameHeader(0, 19, GOAWAY, 0));
  EXPECT_CALL(visitor,
              OnGoAway(5, Http2ErrorCode::ENHANCE_YOUR_CALM, "calm down!!"));
  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  // First stream has received some data.
  EXPECT_GT(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(stream_id1));
  // Second stream was closed.
  EXPECT_EQ(-1, adapter->GetStreamReceiveWindowSize(stream_id2));
  // Third stream has not received any data.
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(stream_id3));

  // Connection window should be the same as the first stream.
  EXPECT_EQ(adapter->GetReceiveWindowSize(),
            adapter->GetStreamReceiveWindowSize(stream_id1));

  // Upper bound on the flow control receive window should still be the initial
  // value.
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowLimit(stream_id1));

  EXPECT_GT(adapter->GetHpackDecoderDynamicTableSize(), 0);

  // Should be 3, but this method only works for server adapters.
  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  // Even though the client recieved a GOAWAY, streams 1 and 5 are still active.
  EXPECT_TRUE(adapter->want_read());

  EXPECT_CALL(visitor, OnFrameHeader(1, 0, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 0));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR))
      .WillOnce(
          [&adapter](Http2StreamId stream_id, Http2ErrorCode /*error_code*/) {
            adapter->RemoveStream(stream_id);
            return true;
          });
  EXPECT_CALL(visitor, OnFrameHeader(5, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnRstStream(5, Http2ErrorCode::REFUSED_STREAM));
  EXPECT_CALL(visitor, OnCloseStream(5, Http2ErrorCode::REFUSED_STREAM))
      .WillOnce(
          [&adapter](Http2StreamId stream_id, Http2ErrorCode /*error_code*/) {
            adapter->RemoveStream(stream_id);
            return true;
          });
  adapter->ProcessBytes(TestFrameSequence()
                            .Data(1, "", true)
                            .RstStream(5, Http2ErrorCode::REFUSED_STREAM)
                            .Serialize());

  // Should be 5, but this method only works for server adapters.
  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  // After receiving END_STREAM for 1 and RST_STREAM for 5, the session no
  // longer expects reads.
  EXPECT_FALSE(adapter->want_read());

  // Client will not have anything else to write.
  EXPECT_FALSE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), testing::IsEmpty());
}

TEST(NgHttp2AdapterTest, QueuingWindowUpdateAffectsWindow) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  EXPECT_EQ(adapter->GetReceiveWindowSize(), kInitialFlowControlWindowSize);
  adapter->SubmitWindowUpdate(0, 10000);
  EXPECT_EQ(adapter->GetReceiveWindowSize(),
            kInitialFlowControlWindowSize + 10000);

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);

  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 0, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 0, 4, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);

  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id),
            kInitialFlowControlWindowSize);
  adapter->SubmitWindowUpdate(1, 20000);
  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id),
            kInitialFlowControlWindowSize + 20000);
}

TEST(NgHttp2AdapterTest, AckOfSettingInitialWindowSizeAffectsWindow) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id1 = adapter->SubmitRequest(headers, true, nullptr);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  int64_t parse_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(parse_result));

  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id1),
            kInitialFlowControlWindowSize);
  adapter->SubmitSettings({{INITIAL_WINDOW_SIZE, 80000u}});
  // No update for the first stream, yet.
  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id1),
            kInitialFlowControlWindowSize);

  // Ack of server's initial settings.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  // Outbound SETTINGS containing INITIAL_WINDOW_SIZE.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);

  // Still no update, as a SETTINGS ack has not yet been received.
  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id1),
            kInitialFlowControlWindowSize);

  const std::string settings_ack =
      TestFrameSequence().SettingsAck().Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck);

  parse_result = adapter->ProcessBytes(settings_ack);
  EXPECT_EQ(settings_ack.size(), static_cast<size_t>(parse_result));

  // Stream window has been updated.
  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id1), 80000);

  const std::vector<Header> headers2 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/two"}});
  const int32_t stream_id2 = adapter->SubmitRequest(headers, true, nullptr);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id2, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id2, _, 0x5, 0));
  result = adapter->Send();
  EXPECT_EQ(0, result);

  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(stream_id2), 80000);
}

TEST(NgHttp2AdapterTest, ClientRejects100HeadersWithFin) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "100"}}, /*fin=*/false)
          .Headers(1, {{":status", "100"}}, /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "100"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "100"));
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  1, Http2VisitorInterface::InvalidFrameError::kHttpMessaging));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(RST_STREAM, 1, _, 0x0, 1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientRejectsFinFollowing100Headers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(stream_id1, {{":status", "100"}}, /*fin=*/false)
          .Data(stream_id1, "", /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(stream_id1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(stream_id1));
  EXPECT_CALL(visitor, OnHeaderForStream(stream_id1, ":status", "100"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(stream_id1));

  EXPECT_CALL(visitor, OnFrameHeader(stream_id1, _, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(stream_id1, _));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(RST_STREAM, 1, _, 0x0, 1));
  EXPECT_CALL(visitor,
              OnCloseStream(stream_id1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientRejects100HeadersWithContent) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "100"}},
                   /*fin=*/false)
          .Data(1, "We needed the final headers before data, whoops")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "100"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientRejects100HeadersWithContentLength) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "100"}, {"content-length", "42"}},
                   /*fin=*/false)
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "100"));
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [content-length], value: [42]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

class ResponseCompleteBeforeRequestTest
    : public quiche::test::QuicheTestWithParam<std::tuple<bool, bool>> {
 public:
  bool HasTrailers() const { return std::get<0>(GetParam()); }
  bool HasRstStream() const { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(TrailersAndRstStreamAllCombinations,
                         ResponseCompleteBeforeRequestTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(ResponseCompleteBeforeRequestTest,
       ClientHandlesResponseBeforeRequestComplete) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "POST"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  adapter->SubmitSettings({});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, false, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x0, 0));
  EXPECT_CALL(visitor,
              OnBeforeFrameSent(HEADERS, stream_id1, _, END_HEADERS_FLAG));
  EXPECT_CALL(visitor,
              OnFrameSent(HEADERS, stream_id1, _, END_HEADERS_FLAG, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  // * The server sends a complete response on stream 1 before the client has
  //   finished sending the request.
  //   * If HasTrailers(), the response ends with trailing HEADERS.
  //   * If HasRstStream(), the response is followed by a RST_STREAM NO_ERROR,
  //     as the HTTP/2 spec recommends.
  TestFrameSequence response;
  response.ServerPreface()
      .Headers(1, {{":status", "200"}, {"content-length", "2"}},
               /*fin=*/false)
      .Data(1, "hi", /*fin=*/!HasTrailers(), /*padding_length=*/10);
  if (HasTrailers()) {
    response.Headers(1, {{"my-weird-trailer", "has a value"}}, /*fin=*/true);
  }
  if (HasRstStream()) {
    response.RstStream(1, Http2ErrorCode::HTTP2_NO_ERROR);
  }
  const std::string stream_frames = response.Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // HEADERS for stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "content-length", "2"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  // DATA frame with padding for stream 1
  EXPECT_CALL(visitor,
              OnFrameHeader(1, 2 + 10, DATA, HasTrailers() ? 0x8 : 0x9));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 2 + 10));
  EXPECT_CALL(visitor, OnDataForStream(1, "hi"));
  EXPECT_CALL(visitor, OnDataPaddingLength(1, 10));
  if (HasTrailers()) {
    // Trailers for stream 1
    EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
    EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
    EXPECT_CALL(visitor,
                OnHeaderForStream(1, "my-weird-trailer", "has a value"));
    EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  }
  // END_STREAM for stream 1
  EXPECT_CALL(visitor, OnEndStream(1));
  if (HasRstStream()) {
    EXPECT_CALL(visitor, OnFrameHeader(1, _, RST_STREAM, 0));
    EXPECT_CALL(visitor, OnRstStream(1, Http2ErrorCode::HTTP2_NO_ERROR));
    EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));
  }

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, ACK_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, ACK_FLAG, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({
                                  SpdyFrameType::SETTINGS,
                              }));

  // Stream 1 is done in the request direction.
  if (!HasRstStream()) {
    visitor.AppendPayloadForStream(1, "final fragment");
  }
  visitor.SetEndData(1, true);
  adapter->ResumeStream(1);

  if (!HasRstStream()) {
    EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, END_STREAM_FLAG, 0));
    // The codec reports Stream 1 as closed.
    EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));
  }

  result = adapter->Send();
  EXPECT_EQ(0, result);
}

TEST(NgHttp2AdapterTest, ClientHandles204WithContent) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  const std::vector<Header> headers2 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/two"}});

  const int32_t stream_id2 = adapter->SubmitRequest(headers2, true, nullptr);
  ASSERT_GT(stream_id2, stream_id1);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id2, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id2, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "204"}, {"content-length", "2"}},
                   /*fin=*/false)
          .Data(1, "hi")
          .Headers(3, {{":status", "204"}}, /*fin=*/false)
          .Data(3, "hi")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "204"));
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [content-length], value: [2]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":status", "204"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(3, 2));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::RST_STREAM,
                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientHandles304WithContent) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "304"}, {"content-length", "2"}},
                   /*fin=*/false)
          .Data(1, "hi")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "304"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "content-length", "2"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 2));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientHandles304WithContentLength) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);
  ASSERT_GT(stream_id, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "304"}, {"content-length", "2"}},
                   /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "304"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "content-length", "2"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientHandlesTrailers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.")
          .Headers(1, {{"final-status", "A-OK"}},
                   /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 26, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 26));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the response body."));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "final-status", "A-OK"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientSendsTrailers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const Http2StreamId kStreamId = 1;
  const std::string kBody = "This is an example request body.";
  visitor.AppendPayloadForStream(kStreamId, kBody);
  visitor.SetEndData(kStreamId, false);
  // nghttp2 does not require that the data source indicate the end of data
  // before trailers are enqueued.

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, false, nullptr);
  ASSERT_GT(stream_id1, 0);
  EXPECT_EQ(stream_id1, kStreamId);
  EXPECT_EQ(adapter->sources_size(), 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id1, _, 0x0, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data,
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA}));
  visitor.Clear();

  const std::vector<Header> trailers1 =
      ToHeaders({{"extra-info", "Trailers are weird but good?"}});
  adapter->SubmitTrailer(stream_id1, trailers1);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  data = visitor.data();
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
}

TEST(NgHttp2AdapterTest, ClientHandlesMetadata) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Metadata(0, "Example connection metadata")
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Metadata(1, "Example stream metadata")
          .Data(1, "This is the response body.", true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(0, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(0));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 26, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 26));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the response body."));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientHandlesMetadataWithEmptyPayload) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Metadata(1, "")
          .Data(1, "This is the response body.", true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(3);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the response body."));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));
}

TEST(NgHttp2AdapterTest, ClientHandlesMetadataWithError) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Metadata(0, "Example connection metadata")
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Metadata(1, "Example stream metadata")
          .Data(1, "This is the response body.", true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(0, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(0));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataForStream(1, _))
      .WillOnce(testing::Return(false));
  // Remaining frames are not processed due to the error.
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  // The false return from OnMetadataForStream() results in a connection error.
  EXPECT_EQ(stream_result, NGHTTP2_ERR_CALLBACK_FAILURE);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  EXPECT_TRUE(adapter->want_read());  // Even after an error. Why?
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientHandlesHpackHeaderTableSetting) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 = ToHeaders({
      {":method", "GET"},
      {":scheme", "http"},
      {":authority", "example.com"},
      {":path", "/this/is/request/one"},
      {"x-i-do-not-like", "green eggs and ham"},
      {"x-i-will-not-eat-them", "here or there, in a box, with a fox"},
      {"x-like-them-in-a-house", "no"},
      {"x-like-them-with-a-mouse", "no"},
  });

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  EXPECT_GT(adapter->GetHpackEncoderDynamicTableSize(), 100);

  const std::string stream_frames =
      TestFrameSequence().Settings({{HEADER_TABLE_SIZE, 100u}}).Serialize();
  // Server preface (SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{HEADER_TABLE_SIZE, 100u}));

  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_LE(adapter->GetHpackEncoderDynamicTableSize(), 100);
}

TEST(NgHttp2AdapterTest, ClientHandlesInvalidTrailers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.")
          .Headers(1, {{":bad-status", "9000"}},
                   /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 26, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 26));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the response body."));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [:bad-status], value: [9000]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  // Bad status trailer will cause a PROTOCOL_ERROR. The header is never
  // delivered in an OnHeaderForStream callback.

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, stream_id1, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(RST_STREAM, stream_id1, 4, 0x0, 1));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientRstStreamWhileHandlingHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&adapter]() {
            adapter->SubmitRst(1, Http2ErrorCode::REFUSED_STREAM);
          }),
          testing::Return(OnHeaderResult::HEADER_RST_STREAM)));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, stream_id1, 4, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, stream_id1, 4, 0x0,
                          static_cast<int>(Http2ErrorCode::REFUSED_STREAM)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::REFUSED_STREAM));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientConnectionErrorWhileHandlingHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"))
      .WillOnce(testing::Return(OnHeaderResult::HEADER_CONNECTION_ERROR));
  // Translation to nghttp2 treats this error as a general parsing error.
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(-902 /* NGHTTP2_ERR_CALLBACK_FAILURE */, stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientConnectionErrorWhileHandlingHeadersOnly) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"))
      .WillOnce(testing::Return(OnHeaderResult::HEADER_CONNECTION_ERROR));
  // Translation to nghttp2 treats this error as a general parsing error.
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(-902 /* NGHTTP2_ERR_CALLBACK_FAILURE */, stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientRejectsHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1))
      .WillOnce(testing::Return(false));
  // Rejecting headers leads to a connection error.
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientStartsShutdown) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  EXPECT_FALSE(adapter->want_write());

  // No-op for a client implementation.
  adapter->SubmitShutdownNotice();
  EXPECT_FALSE(adapter->want_write());

  int result = adapter->Send();
  EXPECT_EQ(0, result);

  EXPECT_EQ(visitor.data(), spdy::kHttp2ConnectionHeaderPrefix);
}

TEST(NgHttp2AdapterTest, ClientSubmitsGoAwayAfterRequest) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);
  ASSERT_GT(stream_id, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _,
                                         END_STREAM_FLAG | END_HEADERS_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _,
                                   END_STREAM_FLAG | END_HEADERS_FLAG, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(stream_id, {{":status", "200"}}, /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, HEADERS,
                                     END_HEADERS_FLAG | END_STREAM_FLAG));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnHeaderForStream(stream_id, ":status", "200"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnEndStream(stream_id));
  EXPECT_CALL(visitor,
              OnCloseStream(stream_id, Http2ErrorCode::HTTP2_NO_ERROR));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  // The stream has been processed, but the highest stream ID should remain
  // as-is (as the server did not open a server push stream). Send a GOAWAY with
  // this stream ID.
  EXPECT_EQ(adapter->GetHighestReceivedStreamId(), 0);
  adapter->SubmitGoAway(adapter->GetHighestReceivedStreamId(),
                        Http2ErrorCode::HTTP2_NO_ERROR, "opaque_data");
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, ACK_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, ACK_FLAG, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(GOAWAY, 0, _, 0x0, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ClientReceivesGoAway) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  const std::vector<Header> headers2 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/two"}});

  const int32_t stream_id2 = adapter->SubmitRequest(headers2, true, nullptr);
  ASSERT_GT(stream_id2, stream_id1);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id2, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id2, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data,
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::HEADERS}));
  visitor.Clear();

  // Submit a pending WINDOW_UPDATE for a stream that will be closed due to
  // GOAWAY. The WINDOW_UPDATE should not be sent.
  adapter->SubmitWindowUpdate(3, 42);

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .RstStream(1, Http2ErrorCode::ENHANCE_YOUR_CALM)
          .GoAway(1, Http2ErrorCode::INTERNAL_ERROR, "indigestion")
          .WindowUpdate(0, 42)
          .WindowUpdate(1, 42)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnRstStream(1, Http2ErrorCode::ENHANCE_YOUR_CALM));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::ENHANCE_YOUR_CALM));
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor,
              OnGoAway(1, Http2ErrorCode::INTERNAL_ERROR, "indigestion"));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::REFUSED_STREAM));
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 42));
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  // SETTINGS ack (but only after the enqueue of the seemingly unrelated
  // WINDOW_UPDATE). The WINDOW_UPDATE is not written.
  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientReceivesMultipleGoAways) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string initial_frames =
      TestFrameSequence()
          .ServerPreface()
          .GoAway(kMaxStreamId, Http2ErrorCode::INTERNAL_ERROR, "indigestion")
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor, OnGoAway(kMaxStreamId, Http2ErrorCode::INTERNAL_ERROR,
                                "indigestion"));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(initial_result));

  // Submit a WINDOW_UPDATE for the open stream. Because the stream is below the
  // GOAWAY's last_stream_id, it should be sent.
  adapter->SubmitWindowUpdate(1, 42);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 1, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 1, 4, 0x0, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::WINDOW_UPDATE}));
  visitor.Clear();

  const std::string final_frames =
      TestFrameSequence()
          .GoAway(0, Http2ErrorCode::INTERNAL_ERROR, "indigestion")
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor,
              OnGoAway(0, Http2ErrorCode::INTERNAL_ERROR, "indigestion"));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::REFUSED_STREAM));

  const int64_t final_result = adapter->ProcessBytes(final_frames);
  EXPECT_EQ(final_frames.size(), static_cast<size_t>(final_result));

  EXPECT_FALSE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), testing::IsEmpty());
}

TEST(NgHttp2AdapterTest, ClientReceivesMultipleGoAwaysWithIncreasingStreamId) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  auto source = std::make_unique<TestMetadataSource>(ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}})));
  adapter->SubmitMetadata(stream_id1, 16384u, std::move(source));

  const std::string frames =
      TestFrameSequence()
          .ServerPreface()
          .GoAway(0, Http2ErrorCode::HTTP2_NO_ERROR, "")
          .GoAway(0, Http2ErrorCode::ENHANCE_YOUR_CALM, "")
          .GoAway(1, Http2ErrorCode::INTERNAL_ERROR, "")
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor, OnGoAway(0, Http2ErrorCode::HTTP2_NO_ERROR, ""));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::REFUSED_STREAM));
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor, OnGoAway(0, Http2ErrorCode::ENHANCE_YOUR_CALM, ""));
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(0, Http2VisitorInterface::InvalidFrameError::kProtocol));

  const int64_t frames_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(frames_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ClientReceivesGoAwayWithPendingStreams) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  EXPECT_THAT(visitor.data(),
              testing::StrEq(spdy::kHttp2ConnectionHeaderPrefix));
  visitor.Clear();

  testing::InSequence s;

  const std::string initial_frames =
      TestFrameSequence()
          .ServerPreface({{MAX_CONCURRENT_STREAMS, 1}})
          .Serialize();

  // Server preface (SETTINGS with MAX_CONCURRENT_STREAMS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting);
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  const std::vector<Header> headers2 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/two"}});

  const int32_t stream_id2 = adapter->SubmitRequest(headers2, true, nullptr);
  ASSERT_GT(stream_id2, stream_id1);

  // The second request should be pending because of
  // SETTINGS_MAX_CONCURRENT_STREAMS.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
  visitor.Clear();

  // Let the client receive a GOAWAY and raise MAX_CONCURRENT_STREAMS. Even
  // though the GOAWAY last_stream_id is higher than the pending request's
  // stream ID, pending request should not be sent.
  const std::string stream_frames =
      TestFrameSequence()
          .GoAway(kMaxStreamId, Http2ErrorCode::INTERNAL_ERROR, "indigestion")
          .Settings({{MAX_CONCURRENT_STREAMS, 42u}})
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor, OnGoAway(kMaxStreamId, Http2ErrorCode::INTERNAL_ERROR,
                                "indigestion"));
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{MAX_CONCURRENT_STREAMS, 42u}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  // Nghttp2 closes the pending stream on the next write attempt.
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::REFUSED_STREAM));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  // Requests submitted after receiving the GOAWAY should not be sent.
  const std::vector<Header> headers3 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/three"}});

  const int32_t stream_id3 = adapter->SubmitRequest(headers3, true, nullptr);
  ASSERT_GT(stream_id3, stream_id2);

  // Nghttp2 closes the pending stream on the next write attempt.
  EXPECT_CALL(visitor, OnCloseStream(5, Http2ErrorCode::REFUSED_STREAM));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), testing::IsEmpty());
  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ClientFailsOnGoAway) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});

  const char* kSentinel1 = "arbitrary pointer 1";
  const int32_t stream_id1 =
      adapter->SubmitRequest(headers1, true, const_cast<char*>(kSentinel1));
  ASSERT_GT(stream_id1, 0);
  QUICHE_LOG(INFO) << "Created stream: " << stream_id1;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .GoAway(1, Http2ErrorCode::INTERNAL_ERROR, "indigestion")
          .Data(1, "This is the response body.")
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "server", "my-fake-server"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(1, "date", "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor,
              OnGoAway(1, Http2ErrorCode::INTERNAL_ERROR, "indigestion"))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ClientRejects101Response) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"},
                 {"upgrade", "new-protocol"}});

  const int32_t stream_id1 = adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string stream_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1,
                   {{":status", "101"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [:status], value: [101]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(static_cast<int64_t>(stream_frames.size()), stream_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, 4, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(RST_STREAM, 1, 4, 0x0,
                  static_cast<uint32_t>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientSubmitRequest) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  EXPECT_THAT(visitor.data(),
              testing::StrEq(spdy::kHttp2ConnectionHeaderPrefix));
  visitor.Clear();

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  testing::InSequence s;

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  EXPECT_EQ(0, adapter->GetHpackEncoderDynamicTableSize());
  EXPECT_FALSE(adapter->want_write());
  const char* kSentinel = "";
  const absl::string_view kBody = "This is an example request body.";
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, true);
  int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, const_cast<char*>(kSentinel));
  ASSERT_EQ(1, stream_id);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, _, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);

  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(stream_id));
  EXPECT_EQ(kInitialFlowControlWindowSize, adapter->GetReceiveWindowSize());
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowLimit(stream_id));

  EXPECT_GT(adapter->GetHpackEncoderDynamicTableSize(), 0);

  // Some data was sent, so the remaining send window size should be less than
  // the default.
  EXPECT_LT(adapter->GetStreamSendWindowSize(stream_id),
            kInitialFlowControlWindowSize);
  EXPECT_GT(adapter->GetStreamSendWindowSize(stream_id), 0);
  // Send window for a nonexistent stream is not available.
  EXPECT_EQ(-1, adapter->GetStreamSendWindowSize(stream_id + 2));

  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA}));
  EXPECT_THAT(visitor.data(), testing::HasSubstr(kBody));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             true, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());
  const char* kSentinel2 = "arbitrary pointer 2";
  EXPECT_EQ(nullptr, adapter->GetStreamUserData(stream_id));
  adapter->SetStreamUserData(stream_id, const_cast<char*>(kSentinel2));

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));

  EXPECT_EQ(kSentinel2, adapter->GetStreamUserData(stream_id));

  // No data was sent (just HEADERS), so the remaining send window size should
  // still be the default.
  EXPECT_EQ(adapter->GetStreamSendWindowSize(stream_id),
            kInitialFlowControlWindowSize);
}

// This test verifies how nghttp2 behaves when a data source becomes
// read-blocked.
TEST(NgHttp2AdapterTest, ClientSubmitRequestWithReadBlock) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const absl::string_view kBody = "This is an example request body.";

  int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  // Resume the deferred stream.
  visitor.AppendPayloadForStream(stream_id, kBody);
  visitor.SetEndData(stream_id, true);
  EXPECT_TRUE(adapter->ResumeStream(stream_id));
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, _, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::DATA}));
  EXPECT_FALSE(adapter->want_write());

  // Stream data is done, so this stream cannot be resumed.
  EXPECT_FALSE(adapter->ResumeStream(stream_id));
  EXPECT_FALSE(adapter->want_write());
}

// This test verifies how nghttp2 behaves when a data source is read block, then
// ends with an empty DATA frame.
TEST(NgHttp2AdapterTest, ClientSubmitRequestEmptyDataWithFin) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  // Resume the deferred stream.
  visitor.SetEndData(stream_id, true);
  EXPECT_TRUE(adapter->ResumeStream(stream_id));
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, 0, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::DATA}));
  EXPECT_FALSE(adapter->want_write());

  // Stream data is done, so this stream cannot be resumed.
  EXPECT_FALSE(adapter->ResumeStream(stream_id));
  EXPECT_FALSE(adapter->want_write());
}

// This test verifies how nghttp2 behaves when a connection becomes
// write-blocked while sending HEADERS.
TEST(NgHttp2AdapterTest, ClientSubmitRequestWithWriteBlock) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  // Flushes the connection preface.
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  EXPECT_EQ(visitor.data(), spdy::kHttp2ConnectionHeaderPrefix);
  visitor.Clear();

  const absl::string_view kBody = "This is an example request body.";

  int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  visitor.set_is_write_blocked(true);

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  result = adapter->Send();

  EXPECT_EQ(0, result);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, _, 0x1, 0));

  visitor.AppendPayloadForStream(stream_id, kBody);
  visitor.SetEndData(stream_id, true);
  visitor.set_is_write_blocked(false);
  result = adapter->Send();
  EXPECT_EQ(0, result);

  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA}));
  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ClientReceivesDataOnClosedStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  EXPECT_THAT(visitor.data(),
              testing::StrEq(spdy::kHttp2ConnectionHeaderPrefix));
  visitor.Clear();

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  testing::InSequence s;

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  // Client SETTINGS ack
  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  // Let the client open a stream with a request.
  int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "GET"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             true, nullptr);
  EXPECT_GT(stream_id, 0);

  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  // Let the client RST_STREAM the stream it opened.
  adapter->SubmitRst(stream_id, Http2ErrorCode::CANCEL);
  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, stream_id, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(RST_STREAM, stream_id, _, 0x0,
                                   static_cast<int>(Http2ErrorCode::CANCEL)));
  EXPECT_CALL(visitor, OnCloseStream(stream_id, Http2ErrorCode::CANCEL));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::RST_STREAM}));
  visitor.Clear();

  // Let the server send a response on the stream. (It might not have received
  // the RST_STREAM yet.)
  const std::string response_frames =
      TestFrameSequence()
          .Headers(stream_id,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(stream_id, "This is the response body.", /*fin=*/true)
          .Serialize();

  // The visitor gets notified about the HEADERS frame but not the DATA frame on
  // the closed stream. No further processing for either frame occurs.
  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, HEADERS, 0x4));
  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, DATA, _)).Times(0);

  const int64_t response_result = adapter->ProcessBytes(response_frames);
  EXPECT_EQ(response_frames.size(), response_result);

  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ClientQueuesRequests) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  adapter->SubmitSettings({});

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x0, 0));
  adapter->Send();

  const std::string initial_frames =
      TestFrameSequence()
          .ServerPreface({{MAX_CONCURRENT_STREAMS, 2}})
          .SettingsAck()
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0x0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{
                           Http2KnownSettingsId::MAX_CONCURRENT_STREAMS, 2u}));
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck());

  adapter->ProcessBytes(initial_frames);

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/example/request"}});
  std::vector<int32_t> stream_ids;
  // Start two, which hits the limit.
  int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);
  stream_ids.push_back(stream_id);
  stream_id = adapter->SubmitRequest(headers, true, nullptr);
  stream_ids.push_back(stream_id);
  // Start two more, which must be queued.
  stream_id = adapter->SubmitRequest(headers, true, nullptr);
  stream_ids.push_back(stream_id);
  stream_id = adapter->SubmitRequest(headers, true, nullptr);
  stream_ids.push_back(stream_id);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_ids[0], _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_ids[0], _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_ids[1], _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_ids[1], _, 0x5, 0));

  adapter->Send();

  const std::string update_streams =
      TestFrameSequence().Settings({{MAX_CONCURRENT_STREAMS, 5}}).Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0x0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{
                           Http2KnownSettingsId::MAX_CONCURRENT_STREAMS, 5u}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  adapter->ProcessBytes(update_streams);

  stream_id = adapter->SubmitRequest(headers, true, nullptr);
  stream_ids.push_back(stream_id);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_ids[2], _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_ids[2], _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_ids[3], _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_ids[3], _, 0x5, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_ids[4], _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_ids[4], _, 0x5, 0));
  // Header frames should all have been sent in order, regardless of any
  // queuing.

  adapter->Send();
}

TEST(NgHttp2AdapterTest, ClientAcceptsHeadResponseWithContentLength) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const std::vector<Header> headers = ToHeaders({{":method", "HEAD"},
                                                 {":scheme", "http"},
                                                 {":authority", "example.com"},
                                                 {":path", "/"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);

  testing::InSequence s;

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  adapter->Send();

  const std::string initial_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(stream_id, {{":status", "200"}, {"content-length", "101"}},
                   /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, _, SETTINGS, 0x0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(2);
  EXPECT_CALL(visitor, OnEndHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnEndStream(stream_id));
  EXPECT_CALL(visitor,
              OnCloseStream(stream_id, Http2ErrorCode::HTTP2_NO_ERROR));

  adapter->ProcessBytes(initial_frames);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  adapter->Send();
}

class MetadataApiTest : public quiche::test::QuicheTestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(WithAndWithoutNewApi, MetadataApiTest,
                         testing::Bool());

TEST_P(MetadataApiTest, SubmitMetadata) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const quiche::HttpHeaderBlock block = ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}}));
  if (GetParam()) {
    visitor.AppendMetadataForStream(1, block);
    adapter->SubmitMetadata(1, 1);
  } else {
    auto source = std::make_unique<TestMetadataSource>(block);
    adapter->SubmitMetadata(1, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x4, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized,
              EqualsFrames({static_cast<SpdyFrameType>(kMetadataFrameType)}));
  EXPECT_FALSE(adapter->want_write());
}

size_t DivRoundUp(size_t numerator, size_t denominator) {
  return numerator / denominator + (numerator % denominator == 0 ? 0 : 1);
}

TEST_P(MetadataApiTest, SubmitMetadataMultipleFrames) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const auto kLargeValue = std::string(63 * 1024, 'a');
  const quiche::HttpHeaderBlock block =
      ToHeaderBlock(ToHeaders({{"large-value", kLargeValue}}));
  if (GetParam()) {
    visitor.AppendMetadataForStream(1, block);
    adapter->SubmitMetadata(1, DivRoundUp(kLargeValue.size(), 16384u));
  } else {
    auto source = std::make_unique<TestMetadataSource>(block);
    adapter->SubmitMetadata(1, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  testing::InSequence seq;
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x4, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized,
              EqualsFrames({static_cast<SpdyFrameType>(kMetadataFrameType),
                            static_cast<SpdyFrameType>(kMetadataFrameType),
                            static_cast<SpdyFrameType>(kMetadataFrameType),
                            static_cast<SpdyFrameType>(kMetadataFrameType)}));
  EXPECT_FALSE(adapter->want_write());
}

TEST_P(MetadataApiTest, SubmitConnectionMetadata) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const quiche::HttpHeaderBlock block = ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}}));
  if (GetParam()) {
    visitor.AppendMetadataForStream(0, block);
    adapter->SubmitMetadata(0, 1);
  } else {
    auto source = std::make_unique<TestMetadataSource>(block);
    adapter->SubmitMetadata(0, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 0, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 0, _, 0x4, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized,
              EqualsFrames({static_cast<SpdyFrameType>(kMetadataFrameType)}));
  EXPECT_FALSE(adapter->want_write());
}

TEST_P(MetadataApiTest, ClientSubmitMetadataWithGoaway) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  adapter->SubmitSettings({});

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, _, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, _, _, 0x0, 0));
  adapter->Send();

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);

  const quiche::HttpHeaderBlock block = ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}}));
  if (GetParam()) {
    visitor.AppendMetadataForStream(stream_id, block);
    adapter->SubmitMetadata(stream_id, 1);
  } else {
    auto source = std::make_unique<TestMetadataSource>(block);
    adapter->SubmitMetadata(stream_id, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  const std::string initial_frames =
      TestFrameSequence()
          .ServerPreface()
          .GoAway(3, Http2ErrorCode::HTTP2_NO_ERROR, "server shutting down")
          .Serialize();
  testing::InSequence s;

  // Server preface
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0));
  EXPECT_CALL(visitor, OnGoAway(3, Http2ErrorCode::HTTP2_NO_ERROR, _));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  // HEADERS frame is not sent.
  EXPECT_CALL(visitor,
              OnBeforeFrameSent(kMetadataFrameType, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, stream_id, _, 0x4, 0));
  EXPECT_CALL(visitor,
              OnCloseStream(stream_id, Http2ErrorCode::REFUSED_STREAM));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized,
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS,
                            static_cast<SpdyFrameType>(kMetadataFrameType)}));
  EXPECT_FALSE(adapter->want_write());
}

TEST_P(MetadataApiTest, ClientSubmitMetadataWithFailureBefore) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  adapter->SubmitSettings({});

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, _, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, _, _, 0x0, 0));
  adapter->Send();

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);

  const quiche::HttpHeaderBlock block = ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}}));
  if (GetParam()) {
    visitor.AppendMetadataForStream(stream_id, block);
    adapter->SubmitMetadata(stream_id, 1);
  } else {
    auto source = std::make_unique<TestMetadataSource>(block);
    adapter->SubmitMetadata(stream_id, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  testing::InSequence s;

  // Server preface
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, stream_id, _, 0x4))
      .WillOnce(testing::Return(NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE));
  EXPECT_CALL(visitor, OnConnectionError(
                           Http2VisitorInterface::ConnectionError::kSendError));

  int result = adapter->Send();
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized,
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS}));
}

TEST_P(MetadataApiTest, ClientSubmitMetadataWithFailureDuring) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  adapter->SubmitSettings({});

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, _, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, _, _, 0x0, 0));
  adapter->Send();

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);

  const quiche::HttpHeaderBlock block = ToHeaderBlock(
      ToHeaders({{"more-than-one-frame", std::string(20000, 'a')}}));
  if (GetParam()) {
    visitor.AppendMetadataForStream(stream_id, block);
    adapter->SubmitMetadata(stream_id, 2);
  } else {
    auto source = std::make_unique<TestMetadataSource>(block);
    adapter->SubmitMetadata(stream_id, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  testing::InSequence s;

  // Server preface
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor,
              OnBeforeFrameSent(kMetadataFrameType, stream_id, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, stream_id, _, 0x0, 0))
      .WillOnce(testing::Return(NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE));
  EXPECT_CALL(visitor, OnConnectionError(
                           Http2VisitorInterface::ConnectionError::kSendError));

  int result = adapter->Send();
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized,
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS,
                            static_cast<SpdyFrameType>(kMetadataFrameType)}));
}

TEST_P(MetadataApiTest, ClientSubmitMetadataWithFailureSending) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  adapter->SubmitSettings({});

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, _, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, _, _, 0x0, 0));
  adapter->Send();

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);

  if (GetParam()) {
    // The test visitor returns an error if no metadata payload is found for the
    // stream.
    adapter->SubmitMetadata(stream_id, 2);
  } else {
    auto source = std::make_unique<TestMetadataSource>(ToHeaderBlock(
        ToHeaders({{"more-than-one-frame", std::string(20000, 'a')}})));
    source->InjectFailure();
    adapter->SubmitMetadata(stream_id, 16384u, std::move(source));
  }
  EXPECT_TRUE(adapter->want_write());

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  testing::InSequence s;

  // Server preface
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnConnectionError(
                           Http2VisitorInterface::ConnectionError::kSendError));

  int result = adapter->Send();
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized, EqualsFrames({
                              SpdyFrameType::SETTINGS,
                              SpdyFrameType::SETTINGS,
                          }));
}

TEST(NgHttp2AdapterTest, ClientObeysMaxConcurrentStreams) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  // Client preface does not appear to include the mandatory SETTINGS frame.
  EXPECT_THAT(visitor.data(),
              testing::StrEq(spdy::kHttp2ConnectionHeaderPrefix));
  visitor.Clear();

  const std::string initial_frames =
      TestFrameSequence()
          .ServerPreface({{MAX_CONCURRENT_STREAMS, 1}})
          .Serialize();
  testing::InSequence s;

  // Server preface (SETTINGS with MAX_CONCURRENT_STREAMS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting);
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  EXPECT_FALSE(adapter->want_write());
  const absl::string_view kBody = "This is an example request body.";
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, true);
  const int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, _, 0x1, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);

  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA}));
  EXPECT_THAT(visitor.data(), testing::HasSubstr(kBody));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  const int next_stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/two"}}),
                             true, nullptr);

  // A new pending stream is created, but because of MAX_CONCURRENT_STREAMS, the
  // session should not want to write it at the moment.
  EXPECT_GT(next_stream_id, stream_id);
  EXPECT_FALSE(adapter->want_write());

  const std::string stream_frames =
      TestFrameSequence()
          .Headers(stream_id,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/false)
          .Data(stream_id, "This is the response body.", /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnHeaderForStream(stream_id, ":status", "200"));
  EXPECT_CALL(visitor,
              OnHeaderForStream(stream_id, "server", "my-fake-server"));
  EXPECT_CALL(visitor, OnHeaderForStream(stream_id, "date",
                                         "Tue, 6 Apr 2021 12:54:01 GMT"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnFrameHeader(stream_id, 26, DATA, 0x1));
  EXPECT_CALL(visitor, OnBeginDataForStream(stream_id, 26));
  EXPECT_CALL(visitor,
              OnDataForStream(stream_id, "This is the response body."));
  EXPECT_CALL(visitor, OnEndStream(stream_id));
  EXPECT_CALL(visitor,
              OnCloseStream(stream_id, Http2ErrorCode::HTTP2_NO_ERROR));

  // The first stream should close, which should make the session want to write
  // the next stream.
  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, next_stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, next_stream_id, _, 0x5, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);

  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ClientReceivesInitialWindowSetting) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const std::string initial_frames =
      TestFrameSequence()
          .Settings({{INITIAL_WINDOW_SIZE, 80000u}})
          .WindowUpdate(0, 65536)
          .Serialize();
  // Server preface (SETTINGS with INITIAL_STREAM_WINDOW)
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{INITIAL_WINDOW_SIZE, 80000u}));
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 65536));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(initial_result));

  // Session will want to write a SETTINGS ack.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  int64_t result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized, EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  const std::string kLongBody = std::string(81000, 'c');
  visitor.AppendPayloadForStream(1, kLongBody);
  visitor.SetEndData(1, true);
  const int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));
  // The client can send more than 4 frames (65536 bytes) of data.
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, 16384, 0x0, 0)).Times(4);
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, 14464, 0x0, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA,
                            SpdyFrameType::DATA, SpdyFrameType::DATA,
                            SpdyFrameType::DATA, SpdyFrameType::DATA}));
}

TEST(NgHttp2AdapterTest, ClientReceivesInitialWindowSettingAfterStreamStart) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().WindowUpdate(0, 65536).Serialize();
  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 65536));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(initial_result));

  // Session will want to write a SETTINGS ack.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  int64_t result = adapter->Send();
  EXPECT_EQ(0, result);
  visitor.Clear();

  const std::string kLongBody = std::string(81000, 'c');
  visitor.AppendPayloadForStream(1, kLongBody);
  visitor.SetEndData(1, true);
  const int stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "POST"},
                                        {":scheme", "http"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             false, nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x4, 0));
  // The client can only send 65535 bytes of data, as the stream window has not
  // yet been increased.
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, 16384, 0x0, 0)).Times(3);
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, 16383, 0x0, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA,
                            SpdyFrameType::DATA, SpdyFrameType::DATA,
                            SpdyFrameType::DATA}));
  visitor.Clear();

  // Can't write any more due to flow control.
  EXPECT_FALSE(adapter->want_write());

  const std::string settings_frame =
      TestFrameSequence().Settings({{INITIAL_WINDOW_SIZE, 80000u}}).Serialize();
  // SETTINGS with INITIAL_STREAM_WINDOW
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{INITIAL_WINDOW_SIZE, 80000u}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t settings_result = adapter->ProcessBytes(settings_frame);
  EXPECT_EQ(settings_frame.size(), static_cast<size_t>(settings_result));

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  // The client can write more after receiving the INITIAL_WINDOW_SIZE setting.
  EXPECT_CALL(visitor, OnFrameSent(DATA, stream_id, 14465, 0x0, 0));

  result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::DATA}));
}

TEST(NgHttp2AdapterTest, InvalidInitialWindowSetting) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const uint32_t kTooLargeInitialWindow = 1u << 31;
  const std::string initial_frames =
      TestFrameSequence()
          .Settings({{INITIAL_WINDOW_SIZE, kTooLargeInitialWindow}})
          .Serialize();
  // Server preface (SETTINGS with INITIAL_STREAM_WINDOW)
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  0, Http2VisitorInterface::InvalidFrameError::kFlowControl));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(initial_result));

  // Session will want to write a GOAWAY.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(GOAWAY, 0, _, 0x0,
                  static_cast<int>(Http2ErrorCode::FLOW_CONTROL_ERROR)));

  int64_t result = adapter->Send();
  EXPECT_EQ(0, result);
  absl::string_view serialized = visitor.data();
  EXPECT_THAT(serialized,
              testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  serialized.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(serialized, EqualsFrames({SpdyFrameType::GOAWAY}));
  visitor.Clear();
}

TEST(NgHttp2AdapterTest, InitialWindowSettingCausesOverflow) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  testing::InSequence s;

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);
  ASSERT_GT(stream_id, 0);
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));
  int64_t write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const uint32_t kLargeInitialWindow = (1u << 31) - 1;
  const std::string frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(stream_id, {{":status", "200"}}, /*fin=*/false)
          .WindowUpdate(stream_id, 65536u)
          .Settings({{INITIAL_WINDOW_SIZE, kLargeInitialWindow}})
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, HEADERS, 0x4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(stream_id));
  EXPECT_CALL(visitor, OnHeaderForStream(stream_id, ":status", "200"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(stream_id));

  EXPECT_CALL(visitor, OnFrameHeader(stream_id, 4, WINDOW_UPDATE, 0x0));
  EXPECT_CALL(visitor, OnWindowUpdate(stream_id, 65536));

  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{INITIAL_WINDOW_SIZE,
                                              kLargeInitialWindow}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  // The stream window update plus the SETTINGS frame with INITIAL_WINDOW_SIZE
  // pushes the stream's flow control window outside of the acceptable range.
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, stream_id, 4, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(RST_STREAM, stream_id, 4, 0x0,
                  static_cast<int>(Http2ErrorCode::FLOW_CONTROL_ERROR)));
  EXPECT_CALL(visitor,
              OnCloseStream(stream_id, Http2ErrorCode::FLOW_CONTROL_ERROR));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS,
                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ClientForbidsPushPromise) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  adapter->SubmitSettings({{ENABLE_PUSH, 0}});

  testing::InSequence s;

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));

  int write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::SETTINGS}));

  visitor.Clear();

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);
  ASSERT_GT(stream_id, 0);
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));
  write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::vector<Header> push_headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/push"}});
  const std::string frames = TestFrameSequence()
                                 .ServerPreface()
                                 .SettingsAck()
                                 .PushPromise(stream_id, 2, push_headers)
                                 .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // SETTINGS ack (to acknowledge PUSH_ENABLED=0)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck);

  // The PUSH_PROMISE is now treated as an invalid frame.
  EXPECT_CALL(visitor, OnFrameHeader(stream_id, _, PUSH_PROMISE, _));
  EXPECT_CALL(visitor, OnInvalidFrame(stream_id, _));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), read_result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(GOAWAY, 0, _, 0x0,
                  static_cast<int32_t>(Http2ErrorCode::PROTOCOL_ERROR)));

  write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
}

TEST(NgHttp2AdapterTest, ClientForbidsPushStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);
  adapter->SubmitSettings({{ENABLE_PUSH, 0}});

  testing::InSequence s;

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));

  int write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data, EqualsFrames({SpdyFrameType::SETTINGS}));

  visitor.Clear();

  const std::vector<Header> headers =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"}});
  const int32_t stream_id = adapter->SubmitRequest(headers, true, nullptr);
  ASSERT_GT(stream_id, 0);
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));
  write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string frames =
      TestFrameSequence()
          .ServerPreface()
          .SettingsAck()
          .Headers(2,
                   {{":status", "200"},
                    {"server", "my-fake-server"},
                    {"date", "Tue, 6 Apr 2021 12:54:01 GMT"}},
                   /*fin=*/true)
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // SETTINGS ack (to acknowledge PUSH_ENABLED=0)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck);

  // The push HEADERS are invalid.
  EXPECT_CALL(visitor, OnFrameHeader(2, _, HEADERS, _));
  EXPECT_CALL(visitor, OnInvalidFrame(2, _));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), read_result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(GOAWAY, 0, _, 0x0,
                  static_cast<int32_t>(Http2ErrorCode::PROTOCOL_ERROR)));

  write_result = adapter->Send();
  EXPECT_EQ(0, write_result);
}

TEST(NgHttp2AdapterTest, FailureSendingConnectionPreface) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  visitor.set_has_write_error();
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kSendError));

  int result = adapter->Send();
  EXPECT_EQ(result, NGHTTP2_ERR_CALLBACK_FAILURE);
}

TEST(NgHttp2AdapterTest, MaxFrameSizeSettingNotAppliedBeforeAck) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const uint32_t large_frame_size = kDefaultFramePayloadSizeLimit + 42;
  adapter->SubmitSettings({{MAX_FRAME_SIZE, large_frame_size}});
  const int32_t stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "GET"},
                                        {":scheme", "https"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             true, /*user_data=*/nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  testing::InSequence s;

  // Client preface (SETTINGS with MAX_FRAME_SIZE) and request HEADERS
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data,
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string server_frames =
      TestFrameSequence()
          .ServerPreface()
          .Headers(1, {{":status", "200"}}, /*fin=*/false)
          .Data(1, std::string(large_frame_size, 'a'))
          .Serialize();

  // Server preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // Response HEADERS. Because the SETTINGS with MAX_FRAME_SIZE was not
  // acknowledged, the large DATA is treated as a connection error. Note that
  // nghttp2 does not deliver any DATA or connection error events.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  const int64_t process_result = adapter->ProcessBytes(server_frames);
  EXPECT_EQ(server_frames.size(), static_cast<size_t>(process_result));

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::FRAME_SIZE_ERROR)));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, MaxFrameSizeSettingAppliedAfterAck) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateClientAdapter(visitor);

  const uint32_t large_frame_size = kDefaultFramePayloadSizeLimit + 42;
  adapter->SubmitSettings({{MAX_FRAME_SIZE, large_frame_size}});
  const int32_t stream_id =
      adapter->SubmitRequest(ToHeaders({{":method", "GET"},
                                        {":scheme", "https"},
                                        {":authority", "example.com"},
                                        {":path", "/this/is/request/one"}}),
                             true, /*user_data=*/nullptr);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(adapter->want_write());

  testing::InSequence s;

  // Client preface (SETTINGS with MAX_FRAME_SIZE) and request HEADERS
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, stream_id, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, stream_id, _, 0x5, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  absl::string_view data = visitor.data();
  EXPECT_THAT(data, testing::StartsWith(spdy::kHttp2ConnectionHeaderPrefix));
  data.remove_prefix(strlen(spdy::kHttp2ConnectionHeaderPrefix));
  EXPECT_THAT(data,
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string server_frames =
      TestFrameSequence()
          .ServerPreface()
          .SettingsAck()
          .Headers(1, {{":status", "200"}}, /*fin=*/false)
          .Data(1, std::string(large_frame_size, 'a'))
          .Serialize();

  // Server preface (empty SETTINGS) and ack of SETTINGS.
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck());

  // Response HEADERS and DATA. Because the SETTINGS with MAX_FRAME_SIZE was
  // acknowledged, the large DATA is accepted without any error.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":status", "200"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, large_frame_size, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, large_frame_size));
  EXPECT_CALL(visitor, OnDataForStream(1, _));

  const int64_t process_result = adapter->ProcessBytes(server_frames);
  EXPECT_EQ(server_frames.size(), static_cast<size_t>(process_result));

  // Client ack of SETTINGS.
  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, WindowUpdateRaisesFlowControlWindowLimit) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string data_chunk(kDefaultFramePayloadSizeLimit, 'a');
  const std::string request = TestFrameSequence()
                                  .ClientPreface()
                                  .Headers(1,
                                           {{":method", "GET"},
                                            {":scheme", "https"},
                                            {":authority", "example.com"},
                                            {":path", "/"}},
                                           /*fin=*/false)
                                  .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  adapter->ProcessBytes(request);

  // Updates the advertised window for the connection and stream 1.
  adapter->SubmitWindowUpdate(0, 2 * kDefaultFramePayloadSizeLimit);
  adapter->SubmitWindowUpdate(1, 2 * kDefaultFramePayloadSizeLimit);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 0, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 0, 4, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 1, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 1, 4, 0x0, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);

  // Verifies the advertised window.
  EXPECT_EQ(kInitialFlowControlWindowSize + 2 * kDefaultFramePayloadSizeLimit,
            adapter->GetReceiveWindowSize());
  EXPECT_EQ(kInitialFlowControlWindowSize + 2 * kDefaultFramePayloadSizeLimit,
            adapter->GetStreamReceiveWindowSize(1));

  const std::string request_body = TestFrameSequence()
                                       .Data(1, data_chunk)
                                       .Data(1, data_chunk)
                                       .Data(1, data_chunk)
                                       .Data(1, data_chunk)
                                       .Data(1, data_chunk)
                                       .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0)).Times(5);
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _)).Times(5);
  EXPECT_CALL(visitor, OnDataForStream(1, _)).Times(5);

  // DATA frames on stream 1 consume most of the window.
  adapter->ProcessBytes(request_body);
  EXPECT_EQ(kInitialFlowControlWindowSize - 3 * kDefaultFramePayloadSizeLimit,
            adapter->GetReceiveWindowSize());
  EXPECT_EQ(kInitialFlowControlWindowSize - 3 * kDefaultFramePayloadSizeLimit,
            adapter->GetStreamReceiveWindowSize(1));

  // Marking the data consumed should result in an advertised window larger than
  // the initial window.
  adapter->MarkDataConsumedForStream(1, 4 * kDefaultFramePayloadSizeLimit);
  EXPECT_GT(adapter->GetReceiveWindowSize(), kInitialFlowControlWindowSize);
  EXPECT_GT(adapter->GetStreamReceiveWindowSize(1),
            kInitialFlowControlWindowSize);
}

TEST(NgHttp2AdapterTest, ConnectionErrorOnControlFrameSent) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence().ClientPreface().Ping(42).Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // PING
  EXPECT_CALL(visitor, OnFrameHeader(0, _, PING, 0));
  EXPECT_CALL(visitor, OnPing(42, false));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  // SETTINGS ack
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0))
      .WillOnce(testing::Return(-902));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kSendError));

  int send_result = adapter->Send();
  EXPECT_LT(send_result, 0);

  // Apparently nghttp2 retries sending the frames that had failed before.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(PING, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(PING, 0, _, 0x1, 0));
  send_result = adapter->Send();
  EXPECT_EQ(send_result, 0);

  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ConnectionErrorOnDataFrameSent) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  visitor.AppendPayloadForStream(
      1, "Here is some data, which will lead to a fatal error");
  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(0, submit_result);

  EXPECT_TRUE(adapter->want_write());

  // SETTINGS ack
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  // Stream 1, with doomed DATA
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0))
      .WillOnce(testing::Return(-902));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kSendError));

  int send_result = adapter->Send();
  EXPECT_LT(send_result, 0);

  // Apparently nghttp2 retries sending the frames that had failed before.
  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));

  send_result = adapter->Send();
  EXPECT_EQ(send_result, 0);

  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ServerConstruction) {
  testing::StrictMock<MockHttp2Visitor> visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  ASSERT_NE(nullptr, adapter);
  EXPECT_TRUE(adapter->want_read());
  EXPECT_FALSE(adapter->want_write());
  EXPECT_TRUE(adapter->IsServerSession());
}

TEST(NgHttp2AdapterTest, ServerHandlesFrames) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());
  EXPECT_EQ(0, adapter->GetHpackDecoderDynamicTableSize());

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

  const char* kSentinel1 = "arbitrary pointer 1";

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(0, 8, PING, 0));
  EXPECT_CALL(visitor, OnPing(42, false));
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 1000));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1))
      .WillOnce(testing::InvokeWithoutArgs([&adapter, kSentinel1]() {
        adapter->SetStreamUserData(1, const_cast<char*>(kSentinel1));
        return true;
      }));
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(1, 2000));
  EXPECT_CALL(visitor, OnFrameHeader(1, 25, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 25));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the request body."));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":scheme", "http"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":path", "/this/is/request/two"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnEndStream(3));
  EXPECT_CALL(visitor, OnFrameHeader(3, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnRstStream(3, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor, OnFrameHeader(0, 8, PING, 0));
  EXPECT_CALL(visitor, OnPing(47, false));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(kSentinel1, adapter->GetStreamUserData(1));

  EXPECT_GT(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowSize(1));
  EXPECT_EQ(adapter->GetStreamReceiveWindowSize(1),
            adapter->GetReceiveWindowSize());
  // Upper bound should still be the original value.
  EXPECT_EQ(kInitialFlowControlWindowSize,
            adapter->GetStreamReceiveWindowLimit(1));

  EXPECT_GT(adapter->GetHpackDecoderDynamicTableSize(), 0);

  // Because stream 3 has already been closed, it's not possible to set user
  // data.
  const char* kSentinel3 = "another arbitrary pointer";
  adapter->SetStreamUserData(3, const_cast<char*>(kSentinel3));
  EXPECT_EQ(nullptr, adapter->GetStreamUserData(3));

  EXPECT_EQ(3, adapter->GetHighestReceivedStreamId());

  EXPECT_EQ(adapter->GetSendWindowSize(), kInitialFlowControlWindowSize + 1000);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(PING, 0, 8, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(PING, 0, 8, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(PING, 0, 8, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(PING, 0, 8, 0x1, 0));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack, two PING acks.
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::PING,
                            SpdyFrameType::PING}));
}

TEST(NgHttp2AdapterTest, ServerVisitorRejectsHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"},
                    {"header1", "ok"},
                    {"header2", "rejected"},
                    {"header3", "not processed"},  // CONTINUATION starts here
                    {"header4", "not processed"},
                    {"header5", "not processed"},
                    {"header6", "not processed"},
                    {"header7", "not processed"},
                    {"header8", "not processed"}},
                   /*fin=*/false, /*add_continuation=*/true)
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x0));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(5);
  EXPECT_CALL(visitor, OnHeaderForStream(1, "header2", _))
      .WillOnce(testing::Return(OnHeaderResult::HEADER_RST_STREAM));
  // The CONTINUATION frame header and header fields are not processed.

  int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::INTERNAL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::INTERNAL_ERROR));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(OgHttp2AdapterTest, HeaderValuesWithObsTextAllowed) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/"},
                                           {"name", "val\xa1ue"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "name", "val\xa1ue"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));
}

TEST(NgHttp2AdapterTest, ServerHandlesDataWithPadding) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .Data(1, "This is the request body.",
                                       /*fin=*/true, /*padding_length=*/39)
                                 .Headers(3,
                                          {{":method", "GET"},
                                           {":scheme", "http"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/two"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 25 + 39, DATA, 0x9));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 25 + 39));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the request body."));
  // Note: nghttp2 passes padding information after the actual data.
  EXPECT_CALL(visitor, OnDataPaddingLength(1, 39));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnEndStream(3));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ServerHandlesHostHeader) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":path", "/this/is/request/one"},
                                           {"host", "example.com"}},
                                          /*fin=*/true)
                                 .Headers(3,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"host", "example.com"}},
                                          /*fin=*/true)
                                 .Headers(5,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "foo.com"},
                                           {":path", "/this/is/request/one"},
                                           {"host", "bar.com"}},
                                          /*fin=*/true)
                                 .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnEndStream(3));

  EXPECT_CALL(visitor, OnFrameHeader(5, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(5));
  EXPECT_CALL(visitor, OnHeaderForStream(5, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(5));
  EXPECT_CALL(visitor, OnEndStream(5));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  visitor.Clear();
}

// Tests the case where the response body is in the progress of being sent while
// trailers are queued.
TEST(NgHttp2AdapterTest, ServerSubmitsTrailersWhileDataDeferred) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .WindowUpdate(0, 2000)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(1, 2000));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor, OnDataForStream(1, "This is the request body."));
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 2000));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  visitor.Clear();

  const absl::string_view kBody = "This is an example response body.";

  // The body source must indicate that the end of the body is not the end of
  // the stream.
  visitor.AppendPayloadForStream(1, kBody);
  int submit_result = adapter->SubmitResponse(
      1, ToHeaders({{":status", "200"}, {"x-comment", "Sure, sounds good."}}),
      false);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));

  send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  int trailer_result =
      adapter->SubmitTrailer(1, ToHeaders({{"final-status", "a-ok"}}));
  ASSERT_EQ(trailer_result, 0);

  // Even though the data source has not finished sending data, nghttp2 will
  // write the trailers anyway.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x5, 0));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
  visitor.Clear();

  // Resuming the stream results in the library wanting to write again.
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, true);
  adapter->ResumeStream(1);
  EXPECT_TRUE(adapter->want_write());

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);

  // But no data is written for the stream.
  EXPECT_THAT(visitor.data(), testing::IsEmpty());
  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ServerSubmitsTrailersWithDataEndStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/one"}})
          .Data(1, "Example data, woohoo.")
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, END_HEADERS_FLAG));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor, OnDataForStream(1, _));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(result), frames.size());

  // Send a body that will end with the END_STREAM flag.
  const absl::string_view kBody = "This is an example response body.";
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, true);

  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(submit_result, 0);

  const std::vector<Header> trailers =
      ToHeaders({{"extra-info", "Trailers are weird but good?"}});
  submit_result = adapter->SubmitTrailer(1, trailers);
  ASSERT_EQ(submit_result, 0);

  // It looks like nghttp2 drops the response body altogether and goes straight
  // to writing the trailers.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, ACK_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, ACK_FLAG, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, END_HEADERS_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, END_HEADERS_FLAG, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _,
                                         END_HEADERS_FLAG | END_STREAM_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _,
                                   END_HEADERS_FLAG | END_STREAM_FLAG, 0));

  const int send_result = adapter->Send();
  EXPECT_EQ(send_result, 0);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS,
                            SpdyFrameType::HEADERS}));
}

TEST(NgHttp2AdapterTest, ServerSubmitsTrailersWithDataEndStreamAndDeferral) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/one"}})
          .Data(1, "Example data, woohoo.")
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, END_HEADERS_FLAG));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor, OnDataForStream(1, _));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(result), frames.size());

  // Send a body that will end with the END_STREAM flag. Don't end the body here
  // so that more body can be added later.
  const absl::string_view kBody = "This is an example response body.";
  visitor.AppendPayloadForStream(1, kBody);

  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(submit_result, 0);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, ACK_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, ACK_FLAG, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, END_HEADERS_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, END_HEADERS_FLAG, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS,
                            SpdyFrameType::DATA}));
  visitor.Clear();

  const std::vector<Header> trailers =
      ToHeaders({{"extra-info", "Trailers are weird but good?"}});
  submit_result = adapter->SubmitTrailer(1, trailers);
  ASSERT_EQ(submit_result, 0);

  // Add more body and signal the end of data. Resuming the stream should allow
  // the new body to be sent, though nghttp2 does not send the body.
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, false);
  adapter->ResumeStream(1);

  // For some reason, nghttp2 drops the new body and goes straight to writing
  // the trailers.
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _,
                                         END_HEADERS_FLAG | END_STREAM_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _,
                                   END_HEADERS_FLAG | END_STREAM_FLAG, 0));

  send_result = adapter->Send();
  EXPECT_EQ(send_result, 0);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
}

TEST(NgHttp2AdapterTest, ClientDisobeysConnectionFlowControl) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"accept", "some bogus value!"}},
                                          /*fin=*/false)
                                 // 70000 bytes of data
                                 .Data(1, std::string(16384, 'a'))
                                 .Data(1, std::string(16384, 'a'))
                                 .Data(1, std::string(16384, 'a'))
                                 .Data(1, std::string(16384, 'a'))
                                 .Data(1, std::string(4464, 'a'))
                                 .Serialize();

  testing::InSequence s;
  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  // No further frame data or headers are delivered.

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  EXPECT_TRUE(adapter->want_write());

  // No SETTINGS ack is written.
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(GOAWAY, 0, _, 0x0,
                  static_cast<int>(Http2ErrorCode::FLOW_CONTROL_ERROR)));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ClientDisobeysConnectionFlowControlWithOneDataFrame) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  // Allow the client to send a DATA frame that exceeds the connection flow
  // control window.
  const uint32_t window_overflow_bytes = kInitialFlowControlWindowSize + 1;
  adapter->SubmitSettings({{MAX_FRAME_SIZE, window_overflow_bytes}});

  const std::string initial_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":method", "POST"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/false)
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  int64_t process_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(process_result));

  EXPECT_TRUE(adapter->want_write());

  // Outbound SETTINGS containing MAX_FRAME_SIZE.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));

  // Ack of client's initial settings.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS}));
  visitor.Clear();

  // Now let the client ack the MAX_FRAME_SIZE SETTINGS and send a DATA frame to
  // overflow the connection-level window. The result should be a GOAWAY.
  const std::string overflow_frames =
      TestFrameSequence()
          .SettingsAck()
          .Data(1, std::string(window_overflow_bytes, 'a'))
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck());
  EXPECT_CALL(visitor, OnFrameHeader(1, window_overflow_bytes, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, window_overflow_bytes));
  // No further frame data is delivered.

  process_result = adapter->ProcessBytes(overflow_frames);
  EXPECT_EQ(overflow_frames.size(), static_cast<size_t>(process_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(GOAWAY, 0, _, 0x0,
                  static_cast<int>(Http2ErrorCode::FLOW_CONTROL_ERROR)));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ClientDisobeysConnectionFlowControlAcrossReads) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  // Allow the client to send a DATA frame that exceeds the connection flow
  // control window.
  const uint32_t window_overflow_bytes = kInitialFlowControlWindowSize + 1;
  adapter->SubmitSettings({{MAX_FRAME_SIZE, window_overflow_bytes}});

  const std::string initial_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":method", "POST"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/false)
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  int64_t process_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), static_cast<size_t>(process_result));

  EXPECT_TRUE(adapter->want_write());

  // Outbound SETTINGS containing MAX_FRAME_SIZE.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));

  // Ack of client's initial settings.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS}));
  visitor.Clear();

  // Now let the client ack the MAX_FRAME_SIZE SETTINGS and send a DATA frame to
  // overflow the connection-level window. The result should be a GOAWAY, but
  // because the processing is split across several calls, nghttp2 instead
  // delivers the data payloads (which the visitor then consumes). This is a bug
  // in nghttp2, which should recognize the flow control error.
  const std::string overflow_frames =
      TestFrameSequence()
          .SettingsAck()
          .Data(1, std::string(window_overflow_bytes, 'a'))
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck());
  EXPECT_CALL(visitor, OnFrameHeader(1, window_overflow_bytes, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, window_overflow_bytes));
  // BUG: The visitor should not have received the data.
  EXPECT_CALL(visitor, OnDataForStream(1, _))
      .WillRepeatedly(
          [&adapter](Http2StreamId stream_id, absl::string_view data) {
            adapter->MarkDataConsumedForStream(stream_id, data.size());
            return true;
          });

  const size_t chunk_length = 16384;
  ASSERT_GE(overflow_frames.size(), chunk_length);
  absl::string_view remaining = overflow_frames;
  while (!remaining.empty()) {
    absl::string_view chunk = remaining.substr(0, chunk_length);
    process_result = adapter->ProcessBytes(chunk);
    EXPECT_EQ(chunk.length(), static_cast<size_t>(process_result));

    remaining.remove_prefix(chunk.length());
  }

  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 0, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 0, 4, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 1, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 1, 4, 0x0, 0));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::WINDOW_UPDATE,
                                            SpdyFrameType::WINDOW_UPDATE}));
}

TEST(NgHttp2AdapterTest, ClientDisobeysStreamFlowControl) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"accept", "some bogus value!"}},
                                          /*fin=*/false)
                                 .Serialize();
  const std::string more_frames = TestFrameSequence()
                                      // 70000 bytes of data
                                      .Data(1, std::string(16384, 'a'))
                                      .Data(1, std::string(16384, 'a'))
                                      .Data(1, std::string(16384, 'a'))
                                      .Data(1, std::string(16384, 'a'))
                                      .Data(1, std::string(4464, 'a'))
                                      .Serialize();

  testing::InSequence s;
  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  adapter->SubmitWindowUpdate(0, 20000);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 0, 4, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 0, 4, 0x0, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::WINDOW_UPDATE}));
  visitor.Clear();

  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, 16384, DATA, 0x0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 16384));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  // No further frame data or headers for stream 1 are delivered.

  result = adapter->ProcessBytes(more_frames);
  EXPECT_EQ(more_frames.size(), static_cast<size_t>(result));

  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, 4, 0x0));
  EXPECT_CALL(
      visitor,
      OnFrameSent(RST_STREAM, 1, 4, 0x0,
                  static_cast<int>(Http2ErrorCode::FLOW_CONTROL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::FLOW_CONTROL_ERROR));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerErrorWhileHandlingHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"accept", "some bogus value!"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .WindowUpdate(0, 2000)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "accept", "some bogus value!"))
      .WillOnce(testing::Return(OnHeaderResult::HEADER_RST_STREAM));
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(1, 2000));
  // DATA frame is not delivered to the visitor.
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 2000));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, 4, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, 4, 0x0,
                          static_cast<int>(Http2ErrorCode::INTERNAL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::INTERNAL_ERROR));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerErrorWhileHandlingHeadersDropsFrames) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"accept", "some bogus value!"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .Metadata(1, "This is the request metadata.")
                                 .RstStream(1, Http2ErrorCode::CANCEL)
                                 .WindowUpdate(0, 2000)
                                 .Headers(3,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/two"}},
                                          /*fin=*/false)
                                 .Metadata(3, "This is the request metadata.",
                                           /*multiple_frames=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnHeaderForStream(1, "accept", "some bogus value!"))
      .WillOnce(testing::Return(OnHeaderResult::HEADER_RST_STREAM));
  // For the RST_STREAM-marked stream, the control frames and METADATA frame but
  // not the DATA frame are delivered to the visitor.
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(1, 2000));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnRstStream(1, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 2000));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, kMetadataFrameType, 0));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(3, _));
  EXPECT_CALL(visitor, OnMetadataForStream(3, "This is the re"))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs([&adapter]() {
                                 adapter->SubmitRst(
                                     3, Http2ErrorCode::REFUSED_STREAM);
                               }),
                               testing::Return(true)));
  // The rest of the metadata is still delivered to the visitor.
  EXPECT_CALL(visitor, OnFrameHeader(3, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(3, _));
  EXPECT_CALL(visitor, OnMetadataForStream(3, "quest metadata."));
  EXPECT_CALL(visitor, OnMetadataEndForStream(3));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, 4, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, 4, 0x0,
                          static_cast<int>(Http2ErrorCode::INTERNAL_ERROR)));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, 4, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, 4, 0x0,
                          static_cast<int>(Http2ErrorCode::REFUSED_STREAM)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::REFUSED_STREAM));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::RST_STREAM,
                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerConnectionErrorWhileHandlingHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"Accept", "uppercase, oh boy!"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .WindowUpdate(0, 2000)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnErrorDebug);
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader))
      .WillOnce(testing::Return(false));
  // Translation to nghttp2 treats this error as a general parsing error.
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(result, NGHTTP2_ERR_CALLBACK_FAILURE);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, 4, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, 4, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack and RST_STREAM
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerErrorAfterHandlingHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .WindowUpdate(0, 2000)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(-902, result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

// Exercises the case when a visitor chooses to reject a frame based solely on
// the frame header, which is a fatal error for the connection.
TEST(NgHttp2AdapterTest, ServerRejectsFrameHeader) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Ping(64)
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .WindowUpdate(1, 2000)
                                 .Data(1, "This is the request body.")
                                 .WindowUpdate(0, 2000)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(0, 8, PING, 0))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(-902, result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ServerRejectsBeginningOfData) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
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
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 25, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 25))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack.
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ServerRejectsStreamData) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
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
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 25, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 25));
  EXPECT_CALL(visitor, OnDataForStream(1, _)).WillOnce(testing::Return(false));
  EXPECT_CALL(visitor, OnConnectionError(ConnectionError::kParseError));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(NGHTTP2_ERR_CALLBACK_FAILURE, result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack.
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ServerReceivesTooLargeHeader) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  // nghttp2 will accept a maximum of 64kB of huffman encoded data per header
  // field.
  const std::string too_large_value = std::string(80 * 1024, 'q');
  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"x-toobig", too_large_value}},
                                          /*fin=*/false)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  // Further header processing is skipped, as the header field is too large.

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_TRUE(adapter->want_write());

  // Since nghttp2 opted not to process the header, it generates a GOAWAY with
  // error code COMPRESSION_ERROR.
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, 8, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, 8, 0x0,
                          static_cast<int>(Http2ErrorCode::COMPRESSION_ERROR)));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // GOAWAY.
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerReceivesInvalidAuthority) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "ex|ample.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [:authority], value: [ex|ample.com]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0x0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, 4, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, 4, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttpAdapterTest, ServerReceivesGoAway) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .GoAway(0, Http2ErrorCode::HTTP2_NO_ERROR, "")
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(0, _, GOAWAY, 0x0));
  EXPECT_CALL(visitor, OnGoAway(0, Http2ErrorCode::HTTP2_NO_ERROR, ""));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<int64_t>(frames.size()), result);

  // The server should still be able to send a response after receiving a GOAWAY
  // with a lower last-stream-ID field, as the stream was client-initiated.
  const int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), true);
  ASSERT_EQ(0, submit_result);

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0x0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
}

TEST(NgHttp2AdapterTest, ServerSubmitResponse) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  const char* kSentinel1 = "arbitrary pointer 1";

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1))
      .WillOnce(testing::InvokeWithoutArgs([&adapter, kSentinel1]() {
        adapter->SetStreamUserData(1, const_cast<char*>(kSentinel1));
        return true;
      }));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(1, adapter->GetHighestReceivedStreamId());

  // Server will want to send a SETTINGS ack.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  EXPECT_EQ(0, adapter->GetHpackEncoderDynamicTableSize());

  EXPECT_FALSE(adapter->want_write());
  const absl::string_view kBody = "This is an example response body.";
  // A data fin is not sent so that the stream remains open, and the flow
  // control state can be verified.
  visitor.AppendPayloadForStream(1, kBody);
  int submit_result = adapter->SubmitResponse(
      1,
      ToHeaders({{":status", "404"},
                 {"x-comment", "I have no idea what you're talking about."}}),
      false);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  // Stream user data should have been set successfully after receiving headers.
  EXPECT_EQ(kSentinel1, adapter->GetStreamUserData(1));
  adapter->SetStreamUserData(1, nullptr);
  EXPECT_EQ(nullptr, adapter->GetStreamUserData(1));

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);

  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA}));
  EXPECT_THAT(visitor.data(), testing::HasSubstr(kBody));
  EXPECT_FALSE(adapter->want_write());

  // Some data was sent, so the remaining send window size should be less than
  // the default.
  EXPECT_LT(adapter->GetStreamSendWindowSize(1), kInitialFlowControlWindowSize);
  EXPECT_GT(adapter->GetStreamSendWindowSize(1), 0);
  // Send window for a nonexistent stream is not available.
  EXPECT_EQ(adapter->GetStreamSendWindowSize(3), -1);

  EXPECT_GT(adapter->GetHpackEncoderDynamicTableSize(), 0);
}

TEST(NgHttp2AdapterTest, ServerSubmitResponseWithResetFromClient) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(1, adapter->GetHighestReceivedStreamId());

  // Server will want to send a SETTINGS ack.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  EXPECT_FALSE(adapter->want_write());
  const absl::string_view kBody = "This is an example response body.";
  visitor.AppendPayloadForStream(1, kBody);
  int submit_result = adapter->SubmitResponse(
      1,
      ToHeaders({{":status", "404"},
                 {"x-comment", "I have no idea what you're talking about."}}),
      false);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());
  EXPECT_EQ(adapter->sources_size(), 0);

  // Client resets the stream before the server can send the response.
  const std::string reset =
      TestFrameSequence().RstStream(1, Http2ErrorCode::CANCEL).Serialize();
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnRstStream(1, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::CANCEL))
      .WillOnce(
          [&adapter](Http2StreamId stream_id, Http2ErrorCode /*error_code*/) {
            adapter->RemoveStream(stream_id);
            return true;
          });
  const int64_t reset_result = adapter->ProcessBytes(reset);
  EXPECT_EQ(reset.size(), static_cast<size_t>(reset_result));

  // The stream's data source is dropped.
  EXPECT_EQ(adapter->sources_size(), 0);

  // Outbound HEADERS and DATA are dropped.
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, _)).Times(0);
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, _, _)).Times(0);
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, _, _)).Times(0);

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);

  EXPECT_THAT(visitor.data(), testing::IsEmpty());
}

// Should also test: client attempts shutdown, server attempts shutdown after an
// explicit GOAWAY.
TEST(NgHttp2AdapterTest, ServerSendsShutdown) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  adapter->SubmitShutdownNotice();

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(GOAWAY, 0, _, 0x0, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerSendsTrailers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  // Server will want to send a SETTINGS ack.
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
  visitor.Clear();

  EXPECT_FALSE(adapter->want_write());
  const absl::string_view kBody = "This is an example response body.";

  // The body source must indicate that the end of the body is not the end of
  // the stream.
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, false);
  int submit_result = adapter->SubmitResponse(
      1, ToHeaders({{":status", "200"}, {"x-comment", "Sure, sounds good."}}),
      false);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::HEADERS, SpdyFrameType::DATA}));
  EXPECT_THAT(visitor.data(), testing::HasSubstr(kBody));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  // The body source has been exhausted by the call to Send() above.
  int trailer_result = adapter->SubmitTrailer(
      1, ToHeaders({{"final-status", "a-ok"},
                    {"x-comment", "trailers sure are cool"}}));
  ASSERT_EQ(trailer_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
}

TEST(NgHttp2AdapterTest, ClientSendsContinuation) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true,
                                          /*add_continuation=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 1));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, CONTINUATION, 4));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);
}

TEST(NgHttp2AdapterTest, ClientSendsMetadataWithContinuation) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .Metadata(0, "Example connection metadata in multiple frames", true)
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/false,
                   /*add_continuation=*/true)
          .Metadata(1,
                    "Some stream metadata that's also sent in multiple frames",
                    true)
          .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Metadata on stream 0
  EXPECT_CALL(visitor, OnFrameHeader(0, _, kMetadataFrameType, 0));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnFrameHeader(0, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataForStream(0, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(0));

  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, CONTINUATION, 4));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  // Metadata on stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, kMetadataFrameType, 0));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, kMetadataFrameType, 4));
  EXPECT_CALL(visitor, OnBeginMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataForStream(1, _));
  EXPECT_CALL(visitor, OnMetadataEndForStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);
  EXPECT_EQ("Example connection metadata in multiple frames",
            absl::StrJoin(visitor.GetMetadata(0), ""));
  EXPECT_EQ("Some stream metadata that's also sent in multiple frames",
            absl::StrJoin(visitor.GetMetadata(1), ""));
}

TEST(NgHttp2AdapterTest, RepeatedHeaderNames) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"},
                                           {"accept", "text/plain"},
                                           {"accept", "text/html"}},
                                          /*fin=*/true)
                                 .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "accept", "text/plain"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "accept", "text/html"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  const std::vector<Header> headers1 = ToHeaders(
      {{":status", "200"}, {"content-length", "10"}, {"content-length", "10"}});
  visitor.AppendPayloadForStream(1, "perfection");
  visitor.SetEndData(1, true);

  int submit_result = adapter->SubmitResponse(1, headers1, false);
  ASSERT_EQ(0, submit_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, 10, 0x1, 0));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS,
                            SpdyFrameType::DATA}));
}

TEST(NgHttp2AdapterTest, ServerRespondsToRequestWithTrailers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/one"}})
          .Data(1, "Example data, woohoo.")
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor, OnDataForStream(1, _));

  int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  const std::vector<Header> headers1 = ToHeaders({{":status", "200"}});

  int submit_result = adapter->SubmitResponse(1, headers1, false);
  ASSERT_EQ(0, submit_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
  visitor.Clear();

  const std::string more_frames =
      TestFrameSequence()
          .Headers(1, {{"extra-info", "Trailers are weird but good?"}},
                   /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "extra-info",
                                         "Trailers are weird but good?"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  result = adapter->ProcessBytes(more_frames);
  EXPECT_EQ(more_frames.size(), static_cast<size_t>(result));

  visitor.SetEndData(1, true);
  EXPECT_EQ(true, adapter->ResumeStream(1));

  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::DATA}));
}

TEST(NgHttp2AdapterTest, ServerSubmitsResponseWithDataSourceError) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  visitor.SimulateError(1);

  int submit_result = adapter->SubmitResponse(
      1, ToHeaders({{":status", "200"}, {"x-comment", "Sure, sounds good."}}),
      false);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(RST_STREAM, 1, _, 0x0, 2));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::INTERNAL_ERROR));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS,
                            SpdyFrameType::RST_STREAM}));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  int trailer_result =
      adapter->SubmitTrailer(1, ToHeaders({{":final-status", "a-ok"}}));
  // The library does not object to the user queuing trailers, even through the
  // stream has already been closed.
  EXPECT_EQ(trailer_result, 0);
}

TEST(NgHttp2AdapterTest, CompleteRequestWithServerResponse) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/false)
          .Data(1, "This is the response body.", /*fin=*/true)
          .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _));
  EXPECT_CALL(visitor, OnDataForStream(1, _));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), true);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, IncompleteRequestWithServerResponse) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));

  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), true);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x5, 0));
  // BUG: Should send RST_STREAM NO_ERROR as well, but nghttp2 does not.

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS}));
  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ServerHandlesMultipleContentLength) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/1"},
                                           {"content-length", "7"},
                                           {"content-length", "7"}},
                                          /*fin=*/false)
                                 .Headers(3,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/3"},
                                           {"content-length", "11"},
                                           {"content-length", "13"}},
                                          /*fin=*/false)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/1"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, "content-length", "7"));
  // nghttp2 does not like duplicate Content-Length headers.
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [content-length], value: [7]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  // Stream 3
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":path", "/3"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, "content-length", "11"));
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 3, name: [content-length], value: [13]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(3, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), static_cast<size_t>(result));
}

TEST(NgHttp2AdapterTest, ServerSendsInvalidTrailers) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  EXPECT_FALSE(adapter->want_write());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  const absl::string_view kBody = "This is an example response body.";

  // The body source must indicate that the end of the body is not the end of
  // the stream.
  visitor.AppendPayloadForStream(1, kBody);
  visitor.SetEndData(1, false);
  int submit_result = adapter->SubmitResponse(
      1, ToHeaders({{":status", "200"}, {"x-comment", "Sure, sounds good."}}),
      false);
  EXPECT_EQ(submit_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::HEADERS,
                            SpdyFrameType::DATA}));
  EXPECT_THAT(visitor.data(), testing::HasSubstr(kBody));
  visitor.Clear();
  EXPECT_FALSE(adapter->want_write());

  // The body source has been exhausted by the call to Send() above.
  int trailer_result =
      adapter->SubmitTrailer(1, ToHeaders({{":final-status", "a-ok"}}));
  ASSERT_EQ(trailer_result, 0);
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x5));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x5, 0));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::HTTP2_NO_ERROR));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::HEADERS}));
}

TEST(NgHttp2AdapterTest, ServerDropsNewStreamBelowWatermark) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(3,
                                          {{":method", "POST"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/false)
                                 .Data(3, "This is the request body.")
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "http"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/two"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":method", "POST"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":scheme", "https"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":authority", "example.com"));
  EXPECT_CALL(visitor, OnHeaderForStream(3, ":path", "/this/is/request/one"));
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnFrameHeader(3, 25, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(3, 25));
  EXPECT_CALL(visitor, OnDataForStream(3, "This is the request body."));

  // It looks like nghttp2 delivers the under-watermark frame header but
  // otherwise silently drops the rest of the frame without error.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnInvalidFrame).Times(0);
  EXPECT_CALL(visitor, OnConnectionError).Times(0);

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(3, adapter->GetHighestReceivedStreamId());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // SETTINGS ack
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterInteractionTest,
     ClientServerInteractionRepeatedHeaderNames) {
  TestVisitor client_visitor;
  auto client_adapter = NgHttp2Adapter::CreateClientAdapter(client_visitor);

  client_adapter->SubmitSettings({});

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"},
                 {"accept", "text/plain"},
                 {"accept", "text/html"}});

  const int32_t stream_id1 =
      client_adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(client_visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x0));
  EXPECT_CALL(client_visitor, OnFrameSent(SETTINGS, 0, _, 0x0, 0));
  EXPECT_CALL(client_visitor, OnBeforeFrameSent(HEADERS, stream_id1, _, 0x5));
  EXPECT_CALL(client_visitor, OnFrameSent(HEADERS, stream_id1, _, 0x5, 0));
  int send_result = client_adapter->Send();
  EXPECT_EQ(0, send_result);

  TestVisitor server_visitor;
  auto server_adapter = NgHttp2Adapter::CreateServerAdapter(server_visitor);

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(server_visitor, OnFrameHeader(0, _, SETTINGS, 0));
  EXPECT_CALL(server_visitor, OnSettingsStart());
  EXPECT_CALL(server_visitor, OnSetting(_)).Times(testing::AnyNumber());
  EXPECT_CALL(server_visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(server_visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(server_visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, ":scheme", "http"));
  EXPECT_CALL(server_visitor,
              OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(server_visitor,
              OnHeaderForStream(1, ":path", "/this/is/request/one"));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, "accept", "text/plain"));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, "accept", "text/html"));
  EXPECT_CALL(server_visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(server_visitor, OnEndStream(1));

  int64_t result = server_adapter->ProcessBytes(client_visitor.data());
  EXPECT_EQ(client_visitor.data().size(), static_cast<size_t>(result));
}

TEST(NgHttp2AdapterInteractionTest, ClientServerInteractionWithCookies) {
  TestVisitor client_visitor;
  auto client_adapter = NgHttp2Adapter::CreateClientAdapter(client_visitor);

  client_adapter->SubmitSettings({});

  const std::vector<Header> headers1 =
      ToHeaders({{":method", "GET"},
                 {":scheme", "http"},
                 {":authority", "example.com"},
                 {":path", "/this/is/request/one"},
                 {"cookie", "a; b=2; c"},
                 {"cookie", "d=e, f, g; h"}});

  const int32_t stream_id1 =
      client_adapter->SubmitRequest(headers1, true, nullptr);
  ASSERT_GT(stream_id1, 0);

  EXPECT_CALL(client_visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x0));
  EXPECT_CALL(client_visitor, OnFrameSent(SETTINGS, 0, _, 0x0, 0));
  EXPECT_CALL(client_visitor,
              OnBeforeFrameSent(HEADERS, stream_id1, _,
                                END_STREAM_FLAG | END_HEADERS_FLAG));
  EXPECT_CALL(client_visitor,
              OnFrameSent(HEADERS, stream_id1, _,
                          END_STREAM_FLAG | END_HEADERS_FLAG, 0));
  int send_result = client_adapter->Send();
  EXPECT_EQ(0, send_result);

  TestVisitor server_visitor;
  auto server_adapter = NgHttp2Adapter::CreateServerAdapter(server_visitor);

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(server_visitor, OnFrameHeader(0, _, SETTINGS, 0));
  EXPECT_CALL(server_visitor, OnSettingsStart());
  EXPECT_CALL(server_visitor, OnSetting).Times(testing::AnyNumber());
  EXPECT_CALL(server_visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(server_visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(server_visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, ":method", "GET"));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, ":scheme", "http"));
  EXPECT_CALL(server_visitor,
              OnHeaderForStream(1, ":authority", "example.com"));
  EXPECT_CALL(server_visitor,
              OnHeaderForStream(1, ":path", "/this/is/request/one"));
  // Cookie values are preserved verbatim.
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, "cookie", "a; b=2; c"));
  EXPECT_CALL(server_visitor, OnHeaderForStream(1, "cookie", "d=e, f, g; h"));
  EXPECT_CALL(server_visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(server_visitor, OnEndStream(1));

  int64_t result = server_adapter->ProcessBytes(client_visitor.data());
  EXPECT_EQ(client_visitor.data().size(), static_cast<size_t>(result));
}

TEST(NgHttp2AdapterTest, ServerForbidsWindowUpdateOnIdleStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  const std::string frames =
      TestFrameSequence().ClientPreface().WindowUpdate(1, 42).Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnInvalidFrame(1, _));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // The GOAWAY apparently causes the SETTINGS ack to be dropped.
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerForbidsDataOnIdleStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Data(1, "Sorry, out of order")
                                 .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  EXPECT_TRUE(adapter->want_write());

  // In this case, nghttp2 goes straight to GOAWAY and does not invoke the
  // invalid frame callback.
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // The GOAWAY apparently causes the SETTINGS ack to be dropped.
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerForbidsRstStreamOnIdleStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  const std::string frames =
      TestFrameSequence()
          .ClientPreface()
          .RstStream(1, Http2ErrorCode::ENHANCE_YOUR_CALM)
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, RST_STREAM, 0));
  EXPECT_CALL(visitor, OnInvalidFrame(1, _));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(frames.size(), result);

  EXPECT_EQ(0, adapter->GetHighestReceivedStreamId());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // The GOAWAY apparently causes the SETTINGS ack to be dropped.
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerForbidsNewStreamAboveStreamLimit) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  adapter->SubmitSettings({{MAX_CONCURRENT_STREAMS, 1}});

  const std::string initial_frames =
      TestFrameSequence().ClientPreface().Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_TRUE(adapter->want_write());

  // Server initial SETTINGS (with MAX_CONCURRENT_STREAMS) and SETTINGS ack.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS}));
  visitor.Clear();

  // Let the client send a SETTINGS ack and then attempt to open more than the
  // advertised number of streams. The overflow stream should be rejected.
  const std::string stream_frames =
      TestFrameSequence()
          .SettingsAck()
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/true)
          .Headers(3,
                   {{":method", "GET"},
                    {":scheme", "http"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/two"}},
                   /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck());
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 0x5));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(3, Http2VisitorInterface::InvalidFrameError::kProtocol));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), stream_result);

  // The server should send a GOAWAY for this error, even though
  // OnInvalidFrame() returns true.
  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerRstStreamsNewStreamAboveStreamLimitBeforeAck) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  adapter->SubmitSettings({{MAX_CONCURRENT_STREAMS, 1}});

  const std::string initial_frames =
      TestFrameSequence().ClientPreface().Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(initial_frames.size(), initial_result);

  EXPECT_TRUE(adapter->want_write());

  // Server initial SETTINGS (with MAX_CONCURRENT_STREAMS) and SETTINGS ack.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS}));
  visitor.Clear();

  // Let the client avoid sending a SETTINGS ack and attempt to open more than
  // the advertised number of streams. The server should still reject the
  // overflow stream, albeit with RST_STREAM REFUSED_STREAM instead of GOAWAY.
  const std::string stream_frames =
      TestFrameSequence()
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/true)
          .Headers(3,
                   {{":method", "GET"},
                    {":scheme", "http"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/two"}},
                   /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 0x5));
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  3, Http2VisitorInterface::InvalidFrameError::kRefusedStream));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_result, stream_frames.size());

  // The server sends a RST_STREAM for the offending stream.
  EXPECT_TRUE(adapter->want_write());
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::REFUSED_STREAM)));

  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, AutomaticSettingsAndPingAcks) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  const std::string frames =
      TestFrameSequence().ClientPreface().Ping(42).Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // PING
  EXPECT_CALL(visitor, OnFrameHeader(0, _, PING, 0));
  EXPECT_CALL(visitor, OnPing(42, false));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  // Server preface does not appear to include the mandatory SETTINGS frame.
  // SETTINGS ack
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  // PING ack
  EXPECT_CALL(visitor, OnBeforeFrameSent(PING, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(PING, 0, _, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::PING}));
}

TEST(NgHttp2AdapterTest, AutomaticPingAcksDisabled) {
  TestVisitor visitor;
  nghttp2_option* options;
  nghttp2_option_new(&options);
  nghttp2_option_set_no_auto_ping_ack(options, 1);
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor, options);
  nghttp2_option_del(options);

  const std::string frames =
      TestFrameSequence().ClientPreface().Ping(42).Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // PING
  EXPECT_CALL(visitor, OnFrameHeader(0, _, PING, 0));
  EXPECT_CALL(visitor, OnPing(42, false));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  // Server preface does not appear to include the mandatory SETTINGS frame.
  // SETTINGS ack
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  // No PING ack expected because automatic PING acks are disabled.

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, InvalidMaxFrameSizeSetting) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence().ClientPreface({{MAX_FRAME_SIZE, 3u}}).Serialize();
  testing::InSequence s;

  // Client preface
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(0, Http2VisitorInterface::InvalidFrameError::kProtocol));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(OgHttp2AdapterTest, InvalidPushSetting) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence().ClientPreface({{ENABLE_PUSH, 3u}}).Serialize();
  testing::InSequence s;

  // Client preface
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnInvalidFrame(0, _));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, InvalidConnectProtocolSetting) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames = TestFrameSequence()
                                 .ClientPreface({{ENABLE_CONNECT_PROTOCOL, 3u}})
                                 .Serialize();
  testing::InSequence s;

  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(0, Http2VisitorInterface::InvalidFrameError::kProtocol));

  int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));

  auto adapter2 = NgHttp2Adapter::CreateServerAdapter(visitor);
  const std::string frames2 = TestFrameSequence()
                                  .ClientPreface({{ENABLE_CONNECT_PROTOCOL, 1}})
                                  .Settings({{ENABLE_CONNECT_PROTOCOL, 0}})
                                  .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSetting(Http2Setting{ENABLE_CONNECT_PROTOCOL, 1u}));
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  // Surprisingly, nghttp2 allows this behavior, which is prohibited in RFC
  // 8441.
  EXPECT_CALL(visitor, OnSetting(Http2Setting{ENABLE_CONNECT_PROTOCOL, 0u}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  read_result = adapter2->ProcessBytes(frames2);
  EXPECT_EQ(static_cast<size_t>(read_result), frames2.size());

  EXPECT_TRUE(adapter2->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  adapter2->Send();
}

TEST(NgHttp2AdapterTest, ServerForbidsProtocolPseudoheaderBeforeAck) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string initial_frames =
      TestFrameSequence().ClientPreface().Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(static_cast<size_t>(initial_result), initial_frames.size());

  // The client attempts to send a CONNECT request with the `:protocol`
  // pseudoheader before receiving the server's SETTINGS frame.
  const std::string stream1_frames =
      TestFrameSequence()
          .Headers(1,
                   {{":method", "CONNECT"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"},
                    {":protocol", "websocket"}},
                   /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [:protocol], value: [websocket]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  int64_t stream_result = adapter->ProcessBytes(stream1_frames);
  EXPECT_EQ(static_cast<size_t>(stream_result), stream1_frames.size());

  // Server sends a SETTINGS ack and initial SETTINGS (with
  // ENABLE_CONNECT_PROTOCOL).
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));

  // The server sends a RST_STREAM for the offending stream.
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));

  adapter->SubmitSettings({{ENABLE_CONNECT_PROTOCOL, 1}});
  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS,
                            SpdyFrameType::RST_STREAM}));
  visitor.Clear();

  // The client attempts to send a CONNECT request with the `:protocol`
  // pseudoheader before acking the server's SETTINGS frame.
  const std::string stream3_frames =
      TestFrameSequence()
          .Headers(3,
                   {{":method", "CONNECT"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/two"},
                    {":protocol", "websocket"}},
                   /*fin=*/true)
          .Serialize();

  // Surprisingly, nghttp2 is okay with this.
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnEndStream(3));

  stream_result = adapter->ProcessBytes(stream3_frames);
  EXPECT_EQ(static_cast<size_t>(stream_result), stream3_frames.size());

  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, ServerAllowsProtocolPseudoheaderAfterAck) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  adapter->SubmitSettings({{ENABLE_CONNECT_PROTOCOL, 1}});

  const std::string initial_frames =
      TestFrameSequence().ClientPreface().Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(static_cast<size_t>(initial_result), initial_frames.size());

  // Server initial SETTINGS (with ENABLE_CONNECT_PROTOCOL) and SETTINGS ack.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  visitor.Clear();

  // The client attempts to send a CONNECT request with the `:protocol`
  // pseudoheader after acking the server's SETTINGS frame.
  const std::string stream_frames =
      TestFrameSequence()
          .SettingsAck()
          .Headers(1,
                   {{":method", "CONNECT"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"},
                    {":protocol", "websocket"}},
                   /*fin=*/true)
          .Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(0, _, SETTINGS, 0x1));
  EXPECT_CALL(visitor, OnSettingsAck());
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(static_cast<size_t>(stream_result), stream_frames.size());

  EXPECT_FALSE(adapter->want_write());
}

TEST(NgHttp2AdapterTest, SkipsSendingFramesForRejectedStream) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string initial_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "http"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/true)
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(static_cast<size_t>(initial_result), initial_frames.size());

  visitor.AppendPayloadForStream(
      1, "Here is some data, which will be completely ignored!");

  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(0, submit_result);

  auto source = std::make_unique<TestMetadataSource>(ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}})));
  adapter->SubmitMetadata(1, 16384u, std::move(source));

  adapter->SubmitWindowUpdate(1, 1024);
  adapter->SubmitRst(1, Http2ErrorCode::INTERNAL_ERROR);

  // Server initial SETTINGS and SETTINGS ack.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  // nghttp2 apparently allows extension frames to be sent on reset streams.
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x4, 0));

  // The server sends a RST_STREAM for the offending stream.
  // The response HEADERS, DATA and WINDOW_UPDATE are all ignored.
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::INTERNAL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::INTERNAL_ERROR));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS,
                            static_cast<SpdyFrameType>(kMetadataFrameType),
                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerQueuesMetadataWithStreamReset) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string initial_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "http"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/false)
          .Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));

  const int64_t initial_result = adapter->ProcessBytes(initial_frames);
  EXPECT_EQ(static_cast<size_t>(initial_result), initial_frames.size());

  visitor.AppendPayloadForStream(
      1, "Here is some data, which will be completely ignored!");

  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(0, submit_result);

  auto source = std::make_unique<TestMetadataSource>(ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}})));
  adapter->SubmitMetadata(1, 16384u, std::move(source));
  adapter->SubmitWindowUpdate(1, 1024);

  const std::string reset_frame =
      TestFrameSequence().RstStream(1, Http2ErrorCode::CANCEL).Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(1, _, RST_STREAM, 0x0));
  EXPECT_CALL(visitor, OnRstStream(1, Http2ErrorCode::CANCEL));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::CANCEL));
  adapter->ProcessBytes(reset_frame);

  source = std::make_unique<TestMetadataSource>(
      ToHeaderBlock(ToHeaders({{"really-important", "information!"}})));
  adapter->SubmitMetadata(1, 16384u, std::move(source));

  EXPECT_EQ(1, adapter->stream_metadata_size());
  EXPECT_EQ(2, adapter->pending_metadata_count(1));

  // Server initial SETTINGS and SETTINGS ack.
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x1, 0));

  // nghttp2 apparently allows extension frames to be sent on reset streams.
  // The response HEADERS, DATA and WINDOW_UPDATE are all discarded.
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(kMetadataFrameType, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(kMetadataFrameType, 1, _, 0x4, 0));

  int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS,
                            static_cast<SpdyFrameType>(kMetadataFrameType),
                            static_cast<SpdyFrameType>(kMetadataFrameType)}));

  EXPECT_EQ(0, adapter->stream_metadata_size());
  EXPECT_EQ(0, adapter->pending_metadata_count(1));
}

TEST(NgHttp2AdapterTest, ServerStartsShutdown) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_FALSE(adapter->want_write());

  adapter->SubmitShutdownNotice();
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(GOAWAY, 0, _, 0x0, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));
}

TEST(NgHttp2AdapterTest, ServerStartsShutdownAfterGoaway) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  EXPECT_FALSE(adapter->want_write());

  adapter->SubmitGoAway(1, Http2ErrorCode::HTTP2_NO_ERROR,
                        "and don't come back!");
  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(GOAWAY, 0, _, 0x0, 0));

  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::GOAWAY}));

  // No-op, since a GOAWAY has previously been enqueued.
  adapter->SubmitShutdownNotice();
  EXPECT_FALSE(adapter->want_write());
}

// Verifies that a connection-level processing error results in repeatedly
// returning a positive value for ProcessBytes() to mark all data as consumed.
TEST(NgHttp2AdapterTest, ConnectionErrorWithBlackholeSinkingData) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence().ClientPreface().WindowUpdate(1, 42).Serialize();

  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnInvalidFrame(1, _));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(result), frames.size());

  // Ask the connection to process more bytes. Because the option is enabled,
  // the data should be marked as consumed.
  const std::string next_frame = TestFrameSequence().Ping(42).Serialize();
  const int64_t next_result = adapter->ProcessBytes(next_frame);
  EXPECT_EQ(static_cast<size_t>(next_result), next_frame.size());
}

TEST(NgHttp2AdapterTest, ServerDoesNotSendFramesAfterImmediateGoAway) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  // Submit a custom initial SETTINGS frame with one setting.
  adapter->SubmitSettings({{HEADER_TABLE_SIZE, 100u}});

  const std::string frames = TestFrameSequence()
                                 .ClientPreface()
                                 .Headers(1,
                                          {{":method", "GET"},
                                           {":scheme", "https"},
                                           {":authority", "example.com"},
                                           {":path", "/this/is/request/one"}},
                                          /*fin=*/true)
                                 .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());
  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  // Submit a response for the stream.
  visitor.AppendPayloadForStream(1, "This data is doomed to never be written.");
  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(0, submit_result);

  // Submit a WINDOW_UPDATE frame.
  adapter->SubmitWindowUpdate(kConnectionStreamId, 42);

  // Submit another SETTINGS frame.
  adapter->SubmitSettings({});

  // Submit some metadata.
  auto source = std::make_unique<TestMetadataSource>(ToHeaderBlock(ToHeaders(
      {{"query-cost", "is too darn high"}, {"secret-sauce", "hollandaise"}})));
  adapter->SubmitMetadata(1, 16384u, std::move(source));

  EXPECT_TRUE(adapter->want_write());

  // Trigger a connection error. Only the response headers will be written.
  const std::string connection_error_frames =
      TestFrameSequence().WindowUpdate(3, 42).Serialize();

  EXPECT_CALL(visitor, OnFrameHeader(3, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnInvalidFrame(3, _));

  const int64_t result = adapter->ProcessBytes(connection_error_frames);
  EXPECT_EQ(static_cast<size_t>(result), connection_error_frames.size());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 6, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 6, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, 0x0, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(GOAWAY, 0, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(GOAWAY, 0, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));

  int send_result = adapter->Send();
  // Some bytes should have been serialized.
  EXPECT_EQ(0, send_result);
  // The GOAWAY apparently causes the other frames to be dropped except for the
  // non-ack SETTINGS frames; nghttp2 sends non-ack SETTINGS frames because they
  // could be the initial SETTINGS frame. However, nghttp2 still allows sending
  // multiple non-ack SETTINGS, which feels non-ideal.
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS,
                            SpdyFrameType::GOAWAY}));
  visitor.Clear();

  // Try to submit more frames for writing. They should not be written.
  adapter->SubmitPing(42);
  EXPECT_FALSE(adapter->want_write());
  send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), testing::IsEmpty());
}

TEST(NgHttp2AdapterTest, ServerHandlesContentLength) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  testing::InSequence s;

  const std::string stream_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/one"},
                       {"content-length", "2"}})
          .Data(1, "hi", /*fin=*/true)
          .Headers(3,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/two"},
                    {"content-length", "nan"}},
                   /*fin=*/true)
          .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // Stream 1: content-length is correct
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 2));
  EXPECT_CALL(visitor, OnDataForStream(1, "hi"));
  EXPECT_CALL(visitor, OnEndStream(1));

  // Stream 3: content-length is not a number
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 3, name: [content-length], value: [nan]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(3, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerHandlesContentLengthMismatch) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  testing::InSequence s;

  const std::string stream_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/two"},
                       {"content-length", "2"}})
          .Data(1, "h", /*fin=*/true)
          .Headers(3, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/three"},
                       {"content-length", "2"}})
          .Data(3, "howdy", /*fin=*/true)
          .Headers(5,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/four"},
                    {"content-length", "2"}},
                   /*fin=*/true)
          .Headers(7,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/four"},
                    {"content-length", "2"}},
                   /*fin=*/false)
          .Data(7, "h", /*fin=*/false)
          .Headers(7, {{"extra-info", "Trailers with content-length mismatch"}},
                   /*fin=*/true)
          .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // Stream 1: content-length is larger than actual data
  // All data is delivered to the visitor, but OnInvalidFrame() is not.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 1));
  EXPECT_CALL(visitor, OnDataForStream(1, "h"));

  // Stream 3: content-length is smaller than actual data
  // The beginning of data is delivered to the visitor, but not the actual data,
  // and neither is OnInvalidFrame().
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, DATA, 1));
  EXPECT_CALL(visitor, OnBeginDataForStream(3, 5));

  // Stream 5: content-length is invalid and HEADERS ends the stream
  // When the stream ends with HEADERS, nghttp2 invokes OnInvalidFrame().
  EXPECT_CALL(visitor, OnFrameHeader(5, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(5));
  EXPECT_CALL(visitor, OnHeaderForStream(5, _, _)).Times(5);
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  5, Http2VisitorInterface::InvalidFrameError::kHttpMessaging));

  // Stream 7: content-length is invalid and trailers end the stream
  // When the stream ends with trailers, nghttp2 invokes OnInvalidFrame().
  EXPECT_CALL(visitor, OnFrameHeader(7, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(7));
  EXPECT_CALL(visitor, OnHeaderForStream(7, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(7));
  EXPECT_CALL(visitor, OnFrameHeader(7, _, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(7, 1));
  EXPECT_CALL(visitor, OnDataForStream(7, "h"));
  EXPECT_CALL(visitor, OnFrameHeader(7, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(7));
  EXPECT_CALL(visitor, OnHeaderForStream(7, _, _));
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  7, Http2VisitorInterface::InvalidFrameError::kHttpMessaging));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 5, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 5, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(5, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 7, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 7, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(7, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(
      visitor.data(),
      EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::RST_STREAM,
                    SpdyFrameType::RST_STREAM, SpdyFrameType::RST_STREAM,
                    SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerHandlesContentLengthMismatchWithDataPending) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);
  // Server preface
  adapter->SubmitSettings({});

  testing::InSequence s;

  const std::string stream_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/one"},
                       {"content-length", "4"}})
          .Data(1, "ok", /*fin=*/false)
          .Headers(3, {{":method", "GET"},
                       {":scheme", "https"},
                       {":authority", "example.com"},
                       {":path", "/this/is/request/three"},
                       {"content-length", "4"}})
          .Data(3, "ok", /*fin=*/false)
          .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // Stream 1
  // Headers and the beginning of data is delivered to the visitor.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, 2, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 2));
  EXPECT_CALL(visitor, OnDataForStream(1, _));

  // Stream 3
  // Headers and the beginning of data is delivered to the visitor.
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(3));
  EXPECT_CALL(visitor, OnFrameHeader(3, 2, DATA, 0));
  EXPECT_CALL(visitor, OnBeginDataForStream(3, 2));
  EXPECT_CALL(visitor, OnDataForStream(3, _));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  // Initial response data for stream 1.
  visitor.AppendPayloadForStream(
      1, "Here is some response data, and there will be more. ");
  adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);

  // Initial response data for stream 3.
  visitor.AppendPayloadForStream(
      3, "Here is some response data, and there will be more. ");
  adapter->SubmitResponse(3, ToHeaders({{":status", "200"}}), false);

  // Server preface (SETTINGS)
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x0));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x0, 0));
  // SETTINGS ack
  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, ACK_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, ACK_FLAG, 0));
  // Stream 1 HEADERS
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  // Stream 3 HEADERS
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 3, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 3, _, 0x4, 0));
  // DATA
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 3, _, 0x0, 0));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(),
              EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::SETTINGS,
                            SpdyFrameType::HEADERS, SpdyFrameType::HEADERS,
                            SpdyFrameType::DATA, SpdyFrameType::DATA}));

  visitor.Clear();

  // Final response data and fin for stream 1.
  visitor.AppendPayloadForStream(1, "Last data!");
  visitor.SetEndData(1, true);
  adapter->ResumeStream(1);

  // Final response data and fin for stream 3.
  visitor.AppendPayloadForStream(3, "Last data!");
  visitor.SetEndData(3, true);
  adapter->ResumeStream(3);

  // Stream 1: actual data overshoots the content-length from request headers.
  // Stream 3: actual data undershoots the content-length from request headers.
  const std::string client_fin = TestFrameSequence()
                                     .Data(1, "ay!", /*fin=*/true)
                                     .Data(3, "", /*fin=*/true)
                                     .Serialize();

  // The library does not deliver the actual data or fin from the client to the
  // visitor.
  EXPECT_CALL(visitor, OnFrameHeader(1, 3, DATA, 0x1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, 3));

  // The library does not deliver the actual data or fin from the client to the
  // visitor.
  EXPECT_CALL(visitor, OnFrameHeader(3, 0, DATA, 0x1));
  EXPECT_CALL(visitor, OnBeginDataForStream(3, 0));

  const int64_t fin_result = adapter->ProcessBytes(client_fin);
  ASSERT_EQ(client_fin.size(), fin_result);

  // The library sends the RST_STREAM but not the end of data.
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));
  // The library sends the RST_STREAM but not the end of data.
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));

  result = adapter->Send();
  EXPECT_EQ(0, result);
}

TEST(NgHttp2AdapterTest, ServerHandlesAsteriskPathForOptions) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  testing::InSequence s;

  const std::string stream_frames = TestFrameSequence()
                                        .ClientPreface()
                                        .Headers(1,
                                                 {{":scheme", "https"},
                                                  {":authority", "example.com"},
                                                  {":path", "*"},
                                                  {":method", "OPTIONS"}},
                                                 /*fin=*/true)
                                        .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS}));
}

TEST(NgHttp2AdapterTest, ServerHandlesInvalidPath) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  testing::InSequence s;

  const std::string stream_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "*"},
                    {":method", "GET"}},
                   /*fin=*/true)
          .Headers(3,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "other/non/slash/starter"},
                    {":method", "GET"}},
                   /*fin=*/true)
          .Headers(5,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", ""},
                    {":method", "GET"}},
                   /*fin=*/true)
          .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  1, Http2VisitorInterface::InvalidFrameError::kHttpMessaging));

  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(4);
  EXPECT_CALL(visitor,
              OnInvalidFrame(
                  3, Http2VisitorInterface::InvalidFrameError::kHttpMessaging));

  EXPECT_CALL(visitor, OnFrameHeader(5, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(5));
  EXPECT_CALL(visitor, OnHeaderForStream(5, _, _)).Times(2);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 5, name: [:path], value: []"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(5, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 5, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 5, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(5, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(
      visitor.data(),
      EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::RST_STREAM,
                    SpdyFrameType::RST_STREAM, SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerHandlesTeHeader) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  testing::InSequence s;

  const std::string stream_frames = TestFrameSequence()
                                        .ClientPreface()
                                        .Headers(1,
                                                 {{":scheme", "https"},
                                                  {":authority", "example.com"},
                                                  {":path", "/"},
                                                  {":method", "GET"},
                                                  {"te", "trailers"}},
                                                 /*fin=*/true)
                                        .Headers(3,
                                                 {{":scheme", "https"},
                                                  {":authority", "example.com"},
                                                  {":path", "/"},
                                                  {":method", "GET"},
                                                  {"te", "trailers, deflate"}},
                                                 /*fin=*/true)
                                        .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // Stream 1: TE: trailers should be allowed.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(5);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  // Stream 3: TE: <non-trailers> should be rejected.
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 3, name: [te], value: [trailers, deflate]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(3, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::RST_STREAM}));
}

TEST(NgHttp2AdapterTest, ServerHandlesConnectionSpecificHeaders) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  testing::InSequence s;

  const std::string stream_frames =
      TestFrameSequence()
          .ClientPreface()
          .Headers(1,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/"},
                    {":method", "GET"},
                    {"connection", "keep-alive"}},
                   /*fin=*/true)
          .Headers(3,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/"},
                    {":method", "GET"},
                    {"proxy-connection", "keep-alive"}},
                   /*fin=*/true)
          .Headers(5,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/"},
                    {":method", "GET"},
                    {"keep-alive", "timeout=42"}},
                   /*fin=*/true)
          .Headers(7,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/"},
                    {":method", "GET"},
                    {"transfer-encoding", "chunked"}},
                   /*fin=*/true)
          .Headers(9,
                   {{":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/"},
                    {":method", "GET"},
                    {"upgrade", "h2c"}},
                   /*fin=*/true)
          .Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  // All streams contain a connection-specific header and should be rejected.
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 1, name: [connection], value: [keep-alive]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(1, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  EXPECT_CALL(visitor, OnFrameHeader(3, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(3));
  EXPECT_CALL(visitor, OnHeaderForStream(3, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 3, name: [proxy-connection], value: [keep-alive]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(3, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  EXPECT_CALL(visitor, OnFrameHeader(5, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(5));
  EXPECT_CALL(visitor, OnHeaderForStream(5, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 5, name: [keep-alive], value: [timeout=42]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(5, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  EXPECT_CALL(visitor, OnFrameHeader(7, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(7));
  EXPECT_CALL(visitor, OnHeaderForStream(7, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 7, name: [transfer-encoding], value: [chunked]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(7, Http2VisitorInterface::InvalidFrameError::kHttpHeader));
  EXPECT_CALL(visitor, OnFrameHeader(9, _, HEADERS, 5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(9));
  EXPECT_CALL(visitor, OnHeaderForStream(9, _, _)).Times(4);
  EXPECT_CALL(
      visitor,
      OnErrorDebug("Invalid HTTP header field was received: frame type: 1, "
                   "stream: 9, name: [upgrade], value: [h2c]"));
  EXPECT_CALL(
      visitor,
      OnInvalidFrame(9, Http2VisitorInterface::InvalidFrameError::kHttpHeader));

  const int64_t stream_result = adapter->ProcessBytes(stream_frames);
  EXPECT_EQ(stream_frames.size(), static_cast<size_t>(stream_result));

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 1, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 1, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(1, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 3, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 3, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(3, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 5, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 5, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(5, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 7, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 7, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(7, Http2ErrorCode::PROTOCOL_ERROR));
  EXPECT_CALL(visitor, OnBeforeFrameSent(RST_STREAM, 9, _, 0x0));
  EXPECT_CALL(visitor,
              OnFrameSent(RST_STREAM, 9, _, 0x0,
                          static_cast<int>(Http2ErrorCode::PROTOCOL_ERROR)));
  EXPECT_CALL(visitor, OnCloseStream(9, Http2ErrorCode::PROTOCOL_ERROR));

  EXPECT_TRUE(adapter->want_write());
  int result = adapter->Send();
  EXPECT_EQ(0, result);
  EXPECT_THAT(
      visitor.data(),
      EqualsFrames({SpdyFrameType::SETTINGS, SpdyFrameType::RST_STREAM,
                    SpdyFrameType::RST_STREAM, SpdyFrameType::RST_STREAM,
                    SpdyFrameType::RST_STREAM, SpdyFrameType::RST_STREAM}));
}

TEST(OgHttp2AdapterTest, ServerConsumesDataWithPadding) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  TestFrameSequence seq = std::move(TestFrameSequence().ClientPreface().Headers(
      1,
      {{":method", "POST"},
       {":scheme", "https"},
       {":authority", "example.com"},
       {":path", "/this/is/request/one"}},
      /*fin=*/false));
  // Generates a bunch of DATA frames, with the bulk of the payloads consisting
  // of padding.
  size_t total_size = 0;
  while (total_size < 62 * 1024) {
    seq.Data(1, "a", /*fin=*/false, /*padding=*/254);
    total_size += 255;
  }
  const std::string frames = seq.Serialize();

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 0, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 4));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnFrameHeader(1, _, DATA, 0x8))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(visitor, OnBeginDataForStream(1, _)).Times(testing::AtLeast(1));
  EXPECT_CALL(visitor, OnDataForStream(1, "a")).Times(testing::AtLeast(1));
  EXPECT_CALL(visitor, OnDataPaddingLength(1, _)).Times(testing::AtLeast(1));

  const int64_t result = adapter->ProcessBytes(frames);
  EXPECT_EQ(result, frames.size());

  EXPECT_TRUE(adapter->want_write());

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, 0, ACK_FLAG));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, 0, ACK_FLAG, 0));
  // Since most of the flow control window consumed is padding, the adapter
  // generates window updates.
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 1, _, 0x0)).Times(1);
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 1, _, 0x0, 0)).Times(1);
  EXPECT_CALL(visitor, OnBeforeFrameSent(WINDOW_UPDATE, 0, _, 0x0)).Times(1);
  EXPECT_CALL(visitor, OnFrameSent(WINDOW_UPDATE, 0, _, 0x0, 0)).Times(1);

  const int send_result = adapter->Send();
  EXPECT_EQ(0, send_result);
  EXPECT_THAT(visitor.data(), EqualsFrames({SpdyFrameType::SETTINGS,
                                            SpdyFrameType::WINDOW_UPDATE,
                                            SpdyFrameType::WINDOW_UPDATE}));
}

TEST(NgHttp2AdapterTest, NegativeFlowControlStreamResumption) {
  TestVisitor visitor;
  auto adapter = NgHttp2Adapter::CreateServerAdapter(visitor);

  const std::string frames =
      TestFrameSequence()
          .ClientPreface({{INITIAL_WINDOW_SIZE, 128u * 1024u}})
          .WindowUpdate(0, 1 << 20)
          .Headers(1,
                   {{":method", "GET"},
                    {":scheme", "https"},
                    {":authority", "example.com"},
                    {":path", "/this/is/request/one"}},
                   /*fin=*/true)
          .Serialize();
  testing::InSequence s;

  // Client preface (empty SETTINGS)
  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor,
              OnSetting(Http2Setting{Http2KnownSettingsId::INITIAL_WINDOW_SIZE,
                                     128u * 1024u}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  EXPECT_CALL(visitor, OnFrameHeader(0, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(0, 1 << 20));

  // Stream 1
  EXPECT_CALL(visitor, OnFrameHeader(1, _, HEADERS, 0x5));
  EXPECT_CALL(visitor, OnBeginHeadersForStream(1));
  EXPECT_CALL(visitor, OnHeaderForStream(1, _, _)).Times(4);
  EXPECT_CALL(visitor, OnEndHeadersForStream(1));
  EXPECT_CALL(visitor, OnEndStream(1));

  const int64_t read_result = adapter->ProcessBytes(frames);
  EXPECT_EQ(static_cast<size_t>(read_result), frames.size());

  // Submit a response for the stream.
  visitor.AppendPayloadForStream(1, std::string(70000, 'a'));
  int submit_result =
      adapter->SubmitResponse(1, ToHeaders({{":status", "200"}}), false);
  ASSERT_EQ(0, submit_result);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  EXPECT_CALL(visitor, OnBeforeFrameSent(HEADERS, 1, _, 0x4));
  EXPECT_CALL(visitor, OnFrameSent(HEADERS, 1, _, 0x4, 0));
  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0)).Times(5);

  adapter->Send();
  EXPECT_FALSE(adapter->want_write());

  EXPECT_CALL(visitor, OnFrameHeader(0, 6, SETTINGS, 0));
  EXPECT_CALL(visitor, OnSettingsStart());
  EXPECT_CALL(visitor,
              OnSetting(Http2Setting{Http2KnownSettingsId::INITIAL_WINDOW_SIZE,
                                     64u * 1024u}));
  EXPECT_CALL(visitor, OnSettingsEnd());

  // Processing these SETTINGS will cause stream 1's send window to become
  // negative.
  adapter->ProcessBytes(TestFrameSequence()
                            .Settings({{INITIAL_WINDOW_SIZE, 64u * 1024u}})
                            .Serialize());
  EXPECT_TRUE(adapter->want_write());
  // nghttp2 does not expose the fact that the send window size is negative.
  EXPECT_EQ(adapter->GetStreamSendWindowSize(1), 0);

  visitor.AppendPayloadForStream(1, "Stream should be resumed.");
  adapter->ResumeStream(1);

  EXPECT_CALL(visitor, OnBeforeFrameSent(SETTINGS, 0, _, 0x1));
  EXPECT_CALL(visitor, OnFrameSent(SETTINGS, 0, _, 0x1, 0));
  adapter->Send();
  EXPECT_FALSE(adapter->want_write());

  // Upon receiving the WINDOW_UPDATE, stream 1 should be ready to write.
  EXPECT_CALL(visitor, OnFrameHeader(1, 4, WINDOW_UPDATE, 0));
  EXPECT_CALL(visitor, OnWindowUpdate(1, 10000));
  adapter->ProcessBytes(TestFrameSequence().WindowUpdate(1, 10000).Serialize());
  EXPECT_TRUE(adapter->want_write());
  EXPECT_GT(adapter->GetStreamSendWindowSize(1), 0);

  EXPECT_CALL(visitor, OnFrameSent(DATA, 1, _, 0x0, 0));
  adapter->Send();
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
