#include "quiche/http2/adapter/event_forwarder.h"

#include <string>

#include "absl/strings/string_view.h"
#include "quiche/http2/adapter/http2_protocol.h"
#include "quiche/http2/core/spdy_protocol.h"
#include "quiche/http2/test_tools/mock_spdy_framer_visitor.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

constexpr absl::string_view some_data = "Here is some data for events";
constexpr spdy::SpdyStreamId stream_id = 1;
constexpr spdy::SpdyErrorCode error_code =
    spdy::SpdyErrorCode::ERROR_CODE_ENHANCE_YOUR_CALM;
constexpr size_t length = 42;

TEST(EventForwarderTest, ForwardsEventsWithTruePredicate) {
  spdy::test::MockSpdyFramerVisitor receiver;
  receiver.DelegateHeaderHandling();
  EventForwarder event_forwarder([]() { return true; }, receiver);

  EXPECT_CALL(
      receiver,
      OnError(Http2DecoderAdapter::SpdyFramerError::SPDY_STOP_PROCESSING,
              std::string(some_data)));
  event_forwarder.OnError(
      Http2DecoderAdapter::SpdyFramerError::SPDY_STOP_PROCESSING,
      std::string(some_data));

  EXPECT_CALL(receiver,
              OnCommonHeader(stream_id, length, /*type=*/0x0, END_STREAM_FLAG));
  event_forwarder.OnCommonHeader(stream_id, length, /*type=*/0x0,
                                 END_STREAM_FLAG);

  EXPECT_CALL(receiver, OnDataFrameHeader(stream_id, length, /*fin=*/true));
  event_forwarder.OnDataFrameHeader(stream_id, length, /*fin=*/true);

  EXPECT_CALL(receiver,
              OnStreamFrameData(stream_id, some_data.data(), some_data.size()));
  event_forwarder.OnStreamFrameData(stream_id, some_data.data(),
                                    some_data.size());

  EXPECT_CALL(receiver, OnStreamEnd(stream_id));
  event_forwarder.OnStreamEnd(stream_id);

  EXPECT_CALL(receiver, OnStreamPadLength(stream_id, length));
  event_forwarder.OnStreamPadLength(stream_id, length);

  EXPECT_CALL(receiver, OnStreamPadding(stream_id, length));
  event_forwarder.OnStreamPadding(stream_id, length);

  EXPECT_CALL(receiver, OnHeaderFrameStart(stream_id));
  spdy::SpdyHeadersHandlerInterface* handler =
      event_forwarder.OnHeaderFrameStart(stream_id);
  EXPECT_EQ(handler, receiver.ReturnTestHeadersHandler(stream_id));

  EXPECT_CALL(receiver, OnHeaderFrameEnd(stream_id));
  event_forwarder.OnHeaderFrameEnd(stream_id);

  EXPECT_CALL(receiver, OnRstStream(stream_id, error_code));
  event_forwarder.OnRstStream(stream_id, error_code);

  EXPECT_CALL(receiver, OnSettings());
  event_forwarder.OnSettings();

  EXPECT_CALL(
      receiver,
      OnSetting(spdy::SpdyKnownSettingsId::SETTINGS_MAX_CONCURRENT_STREAMS,
                100));
  event_forwarder.OnSetting(
      spdy::SpdyKnownSettingsId::SETTINGS_MAX_CONCURRENT_STREAMS, 100);

  EXPECT_CALL(receiver, OnSettingsEnd());
  event_forwarder.OnSettingsEnd();

  EXPECT_CALL(receiver, OnSettingsAck());
  event_forwarder.OnSettingsAck();

  EXPECT_CALL(receiver, OnPing(/*unique_id=*/42, /*is_ack=*/false));
  event_forwarder.OnPing(/*unique_id=*/42, /*is_ack=*/false);

  EXPECT_CALL(receiver, OnGoAway(stream_id, error_code));
  event_forwarder.OnGoAway(stream_id, error_code);

  EXPECT_CALL(receiver, OnGoAwayFrameData(some_data.data(), some_data.size()));
  event_forwarder.OnGoAwayFrameData(some_data.data(), some_data.size());

  EXPECT_CALL(receiver,
              OnHeaders(stream_id, /*payload_length=*/1234,
                        /*has_priority=*/false, /*weight=*/42, stream_id + 2,
                        /*exclusive=*/false, /*fin=*/true, /*end=*/true));
  event_forwarder.OnHeaders(stream_id, /*payload_length=*/1234,
                            /*has_priority=*/false, /*weight=*/42,
                            stream_id + 2, /*exclusive=*/false, /*fin=*/true,
                            /*end=*/true);

  EXPECT_CALL(receiver, OnWindowUpdate(stream_id, /*delta_window_size=*/42));
  event_forwarder.OnWindowUpdate(stream_id, /*delta_window_size=*/42);

  EXPECT_CALL(receiver, OnPushPromise(stream_id, stream_id + 1, /*end=*/true));
  event_forwarder.OnPushPromise(stream_id, stream_id + 1, /*end=*/true);

  EXPECT_CALL(receiver,
              OnContinuation(stream_id, /*payload_length=*/42, /*end=*/true));
  event_forwarder.OnContinuation(stream_id, /*payload_length=*/42,
                                 /*end=*/true);

  const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  EXPECT_CALL(receiver, OnAltSvc(stream_id, some_data, altsvc_vector));
  event_forwarder.OnAltSvc(stream_id, some_data, altsvc_vector);

  EXPECT_CALL(receiver, OnPriority(stream_id, stream_id + 2, /*weight=*/42,
                                   /*exclusive=*/false));
  event_forwarder.OnPriority(stream_id, stream_id + 2, /*weight=*/42,
                             /*exclusive=*/false);

  EXPECT_CALL(receiver, OnPriorityUpdate(stream_id, some_data));
  event_forwarder.OnPriorityUpdate(stream_id, some_data);

  EXPECT_CALL(receiver, OnUnknownFrame(stream_id, /*frame_type=*/0x4D));
  event_forwarder.OnUnknownFrame(stream_id, /*frame_type=*/0x4D);

  EXPECT_CALL(receiver, OnUnknownFrameStart(stream_id, /*length=*/42,
                                            /*type=*/0x4D, /*flags=*/0x0));
  event_forwarder.OnUnknownFrameStart(stream_id, /*length=*/42, /*type=*/0x4D,
                                      /*flags=*/0x0);
}

TEST(EventForwarderTest, DoesNotForwardEventsWithFalsePredicate) {
  spdy::test::MockSpdyFramerVisitor receiver;
  receiver.DelegateHeaderHandling();
  EventForwarder event_forwarder([]() { return false; }, receiver);

  EXPECT_CALL(receiver, OnError).Times(0);
  event_forwarder.OnError(
      Http2DecoderAdapter::SpdyFramerError::SPDY_STOP_PROCESSING,
      std::string(some_data));

  EXPECT_CALL(receiver, OnCommonHeader).Times(0);
  event_forwarder.OnCommonHeader(stream_id, length, /*type=*/0x0,
                                 END_STREAM_FLAG);

  EXPECT_CALL(receiver, OnDataFrameHeader).Times(0);
  event_forwarder.OnDataFrameHeader(stream_id, length, /*fin=*/true);

  EXPECT_CALL(receiver, OnStreamFrameData).Times(0);
  event_forwarder.OnStreamFrameData(stream_id, some_data.data(),
                                    some_data.size());

  EXPECT_CALL(receiver, OnStreamEnd).Times(0);
  event_forwarder.OnStreamEnd(stream_id);

  EXPECT_CALL(receiver, OnStreamPadLength).Times(0);
  event_forwarder.OnStreamPadLength(stream_id, length);

  EXPECT_CALL(receiver, OnStreamPadding).Times(0);
  event_forwarder.OnStreamPadding(stream_id, length);

  EXPECT_CALL(receiver, OnHeaderFrameStart(stream_id));
  spdy::SpdyHeadersHandlerInterface* handler =
      event_forwarder.OnHeaderFrameStart(stream_id);
  EXPECT_EQ(handler, receiver.ReturnTestHeadersHandler(stream_id));

  EXPECT_CALL(receiver, OnHeaderFrameEnd).Times(0);
  event_forwarder.OnHeaderFrameEnd(stream_id);

  EXPECT_CALL(receiver, OnRstStream).Times(0);
  event_forwarder.OnRstStream(stream_id, error_code);

  EXPECT_CALL(receiver, OnSettings).Times(0);
  event_forwarder.OnSettings();

  EXPECT_CALL(receiver, OnSetting).Times(0);
  event_forwarder.OnSetting(
      spdy::SpdyKnownSettingsId::SETTINGS_MAX_CONCURRENT_STREAMS, 100);

  EXPECT_CALL(receiver, OnSettingsEnd).Times(0);
  event_forwarder.OnSettingsEnd();

  EXPECT_CALL(receiver, OnSettingsAck).Times(0);
  event_forwarder.OnSettingsAck();

  EXPECT_CALL(receiver, OnPing).Times(0);
  event_forwarder.OnPing(/*unique_id=*/42, /*is_ack=*/false);

  EXPECT_CALL(receiver, OnGoAway).Times(0);
  event_forwarder.OnGoAway(stream_id, error_code);

  EXPECT_CALL(receiver, OnGoAwayFrameData).Times(0);
  event_forwarder.OnGoAwayFrameData(some_data.data(), some_data.size());

  EXPECT_CALL(receiver, OnHeaders).Times(0);
  event_forwarder.OnHeaders(stream_id, /*payload_length=*/1234,
                            /*has_priority=*/false, /*weight=*/42,
                            stream_id + 2, /*exclusive=*/false, /*fin=*/true,
                            /*end=*/true);

  EXPECT_CALL(receiver, OnWindowUpdate).Times(0);
  event_forwarder.OnWindowUpdate(stream_id, /*delta_window_size=*/42);

  EXPECT_CALL(receiver, OnPushPromise).Times(0);
  event_forwarder.OnPushPromise(stream_id, stream_id + 1, /*end=*/true);

  EXPECT_CALL(receiver, OnContinuation).Times(0);
  event_forwarder.OnContinuation(stream_id, /*payload_length=*/42,
                                 /*end=*/true);

  EXPECT_CALL(receiver, OnAltSvc).Times(0);
  const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  event_forwarder.OnAltSvc(stream_id, some_data, altsvc_vector);

  EXPECT_CALL(receiver, OnPriority).Times(0);
  event_forwarder.OnPriority(stream_id, stream_id + 2, /*weight=*/42,
                             /*exclusive=*/false);

  EXPECT_CALL(receiver, OnPriorityUpdate).Times(0);
  event_forwarder.OnPriorityUpdate(stream_id, some_data);

  EXPECT_CALL(receiver, OnUnknownFrame).Times(0);
  event_forwarder.OnUnknownFrame(stream_id, /*frame_type=*/0x4D);

  EXPECT_CALL(receiver, OnUnknownFrameStart).Times(0);
  event_forwarder.OnUnknownFrameStart(stream_id, /*length=*/42, /*type=*/0x4D,
                                      /*flags=*/0x0);
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
