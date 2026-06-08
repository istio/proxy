#include "quiche/http2/adapter/nghttp2.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "quiche/http2/adapter/mock_nghttp2_callbacks.h"
#include "quiche/http2/adapter/nghttp2_test_utils.h"
#include "quiche/http2/adapter/nghttp2_util.h"
#include "quiche/http2/adapter/test_frame_sequence.h"
#include "quiche/http2/adapter/test_utils.h"
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

nghttp2_option* GetOptions() {
  nghttp2_option* options;
  nghttp2_option_new(&options);
  // Set some common options for compatibility.
  nghttp2_option_set_no_closed_streams(options, 1);
  nghttp2_option_set_no_auto_window_update(options, 1);
  nghttp2_option_set_max_send_header_block_length(options, 0x2000000);
  nghttp2_option_set_max_outbound_ack(options, 10000);
  return options;
}

class Nghttp2Test : public quiche::test::QuicheTest {
 public:
  Nghttp2Test() : session_(MakeSessionPtr(nullptr)) {}

  void SetUp() override { InitializeSession(); }

  virtual Perspective GetPerspective() = 0;

  void InitializeSession() {
    auto nghttp2_callbacks = MockNghttp2Callbacks::GetCallbacks();
    nghttp2_option* options = GetOptions();
    nghttp2_session* ptr;
    if (GetPerspective() == Perspective::kClient) {
      nghttp2_session_client_new2(&ptr, nghttp2_callbacks.get(),
                                  &mock_callbacks_, options);
    } else {
      nghttp2_session_server_new2(&ptr, nghttp2_callbacks.get(),
                                  &mock_callbacks_, options);
    }
    nghttp2_option_del(options);

    // Sets up the Send() callback to append to |serialized_|.
    EXPECT_CALL(mock_callbacks_, Send(_, _, _))
        .WillRepeatedly(
            [this](const uint8_t* data, size_t length, int /*flags*/) {
              absl::StrAppend(&serialized_, ToStringView(data, length));
              return length;
            });
    // Sets up the SendData() callback to fetch and append data from a
    // TestDataSource.
    EXPECT_CALL(mock_callbacks_, SendData(_, _, _, _))
        .WillRepeatedly([this](nghttp2_frame* /*frame*/, const uint8_t* framehd,
                               size_t length, nghttp2_data_source* source) {
          QUICHE_LOG(INFO) << "Appending frame header and " << length
                           << " bytes of data";
          auto* s = static_cast<TestDataSource*>(source->ptr);
          absl::StrAppend(&serialized_, ToStringView(framehd, 9),
                          s->ReadNext(length));
          return 0;
        });
    session_ = MakeSessionPtr(ptr);
  }

  testing::StrictMock<MockNghttp2Callbacks> mock_callbacks_;
  nghttp2_session_unique_ptr session_;
  std::string serialized_;
};

class Nghttp2ClientTest : public Nghttp2Test {
 public:
  Perspective GetPerspective() override { return Perspective::kClient; }
};

// Verifies nghttp2 behavior when acting as a client.
TEST_F(Nghttp2ClientTest, ClientReceivesUnexpectedHeaders) {
  const std::string initial_frames = TestFrameSequence()
                                         .ServerPreface()
                                         .Ping(42)
                                         .WindowUpdate(0, 1000)
                                         .Serialize();

  testing::InSequence seq;
  EXPECT_CALL(mock_callbacks_, OnBeginFrame(HasFrameHeader(0, SETTINGS, 0)));
  EXPECT_CALL(mock_callbacks_, OnFrameRecv(IsSettings(testing::IsEmpty())));
  EXPECT_CALL(mock_callbacks_, OnBeginFrame(HasFrameHeader(0, PING, 0)));
  EXPECT_CALL(mock_callbacks_, OnFrameRecv(IsPing(42)));
  EXPECT_CALL(mock_callbacks_,
              OnBeginFrame(HasFrameHeader(0, WINDOW_UPDATE, 0)));
  EXPECT_CALL(mock_callbacks_, OnFrameRecv(IsWindowUpdate(1000)));

  ssize_t result = nghttp2_session_mem_recv(
      session_.get(), ToUint8Ptr(initial_frames.data()), initial_frames.size());
  ASSERT_EQ(result, initial_frames.size());

  const std::string unexpected_stream_frames =
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

  EXPECT_CALL(mock_callbacks_, OnBeginFrame(HasFrameHeader(1, HEADERS, _)));
  EXPECT_CALL(mock_callbacks_, OnInvalidFrameRecv(IsHeaders(1, _, _), _));
  // No events from the DATA, RST_STREAM or GOAWAY.

  nghttp2_session_mem_recv(session_.get(),
                           ToUint8Ptr(unexpected_stream_frames.data()),
                           unexpected_stream_frames.size());
}

// Tests the request-sending behavior of nghttp2 when acting as a client.
TEST_F(Nghttp2ClientTest, ClientSendsRequest) {
  int result = nghttp2_session_send(session_.get());
  ASSERT_EQ(result, 0);

  EXPECT_THAT(serialized_, testing::StrEq(spdy::kHttp2ConnectionHeaderPrefix));
  serialized_.clear();

  const std::string initial_frames =
      TestFrameSequence().ServerPreface().Serialize();
  testing::InSequence s;

  // Server preface (empty SETTINGS)
  EXPECT_CALL(mock_callbacks_, OnBeginFrame(HasFrameHeader(0, SETTINGS, 0)));
  EXPECT_CALL(mock_callbacks_, OnFrameRecv(IsSettings(testing::IsEmpty())));

  ssize_t recv_result = nghttp2_session_mem_recv(
      session_.get(), ToUint8Ptr(initial_frames.data()), initial_frames.size());
  EXPECT_EQ(initial_frames.size(), recv_result);

  // Client wants to send a SETTINGS ack.
  EXPECT_CALL(mock_callbacks_, BeforeFrameSend(IsSettings(testing::IsEmpty())));
  EXPECT_CALL(mock_callbacks_, OnFrameSend(IsSettings(testing::IsEmpty())));
  EXPECT_TRUE(nghttp2_session_want_write(session_.get()));
  result = nghttp2_session_send(session_.get());
  EXPECT_THAT(serialized_, EqualsFrames({spdy::SpdyFrameType::SETTINGS}));
  serialized_.clear();

  EXPECT_FALSE(nghttp2_session_want_write(session_.get()));

  // The following sets up the client request.
  std::vector<std::pair<absl::string_view, absl::string_view>> headers = {
      {":method", "POST"},
      {":scheme", "http"},
      {":authority", "example.com"},
      {":path", "/this/is/request/one"}};
  std::vector<nghttp2_nv> nvs;
  for (const auto& h : headers) {
    nvs.push_back({.name = ToUint8Ptr(h.first.data()),
                   .value = ToUint8Ptr(h.second.data()),
                   .namelen = h.first.size(),
                   .valuelen = h.second.size(),
                   .flags = NGHTTP2_NV_FLAG_NONE});
  }
  const absl::string_view kBody = "This is an example request body.";
  TestDataSource source{kBody};
  nghttp2_data_provider provider = source.MakeDataProvider();
  // After submitting the request, the client will want to write.
  int stream_id =
      nghttp2_submit_request(session_.get(), nullptr /* pri_spec */, nvs.data(),
                             nvs.size(), &provider, nullptr /* stream_data */);
  EXPECT_GT(stream_id, 0);
  EXPECT_TRUE(nghttp2_session_want_write(session_.get()));

  // We expect that the client will want to write HEADERS, then DATA.
  EXPECT_CALL(mock_callbacks_, BeforeFrameSend(IsHeaders(stream_id, _, _)));
  EXPECT_CALL(mock_callbacks_, OnFrameSend(IsHeaders(stream_id, _, _)));
  EXPECT_CALL(mock_callbacks_, OnFrameSend(IsData(stream_id, kBody.size(), _)));
  nghttp2_session_send(session_.get());
  EXPECT_THAT(serialized_, EqualsFrames({spdy::SpdyFrameType::HEADERS,
                                         spdy::SpdyFrameType::DATA}));
  EXPECT_THAT(serialized_, testing::HasSubstr(kBody));

  // Once the request is flushed, the client no longer wants to write.
  EXPECT_FALSE(nghttp2_session_want_write(session_.get()));
}

class Nghttp2ServerTest : public Nghttp2Test {
 public:
  Perspective GetPerspective() override { return Perspective::kServer; }
};

// Verifies the behavior when a stream ends early.
TEST_F(Nghttp2ServerTest, MismatchedContentLength) {
  const std::string initial_frames =
      TestFrameSequence()
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

  testing::InSequence seq;
  EXPECT_CALL(mock_callbacks_, OnBeginFrame(HasFrameHeader(0, SETTINGS, _)));

  EXPECT_CALL(mock_callbacks_, OnFrameRecv(IsSettings(testing::IsEmpty())));

  // HEADERS on stream 1
  EXPECT_CALL(mock_callbacks_, OnBeginFrame(HasFrameHeader(
                                   1, HEADERS, NGHTTP2_FLAG_END_HEADERS)));

  EXPECT_CALL(mock_callbacks_,
              OnBeginHeaders(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                       NGHTTP2_HCAT_REQUEST)));

  EXPECT_CALL(mock_callbacks_, OnHeader(_, ":method", "POST", _));
  EXPECT_CALL(mock_callbacks_, OnHeader(_, ":scheme", "https", _));
  EXPECT_CALL(mock_callbacks_, OnHeader(_, ":authority", "example.com", _));
  EXPECT_CALL(mock_callbacks_, OnHeader(_, ":path", "/", _));
  EXPECT_CALL(mock_callbacks_, OnHeader(_, "content-length", "50", _));
  EXPECT_CALL(mock_callbacks_,
              OnFrameRecv(IsHeaders(1, NGHTTP2_FLAG_END_HEADERS,
                                    NGHTTP2_HCAT_REQUEST)));

  // DATA on stream 1
  EXPECT_CALL(mock_callbacks_,
              OnBeginFrame(HasFrameHeader(1, DATA, NGHTTP2_FLAG_END_STREAM)));

  EXPECT_CALL(mock_callbacks_, OnDataChunkRecv(NGHTTP2_FLAG_END_STREAM, 1,
                                               "Less than 50 bytes."));

  // No OnFrameRecv() callback for the DATA frame, since there is a
  // Content-Length mismatch error.

  ssize_t result = nghttp2_session_mem_recv(
      session_.get(), ToUint8Ptr(initial_frames.data()), initial_frames.size());
  ASSERT_EQ(result, initial_frames.size());
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
