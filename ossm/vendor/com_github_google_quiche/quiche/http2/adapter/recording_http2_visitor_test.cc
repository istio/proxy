#include "quiche/http2/adapter/recording_http2_visitor.h"

#include <list>
#include <string>

#include "quiche/http2/adapter/http2_protocol.h"
#include "quiche/http2/adapter/http2_visitor_interface.h"
#include "quiche/http2/test_tools/http2_random.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {
namespace {

using ::testing::IsEmpty;

TEST(RecordingHttp2VisitorTest, EmptySequence) {
  RecordingHttp2Visitor chocolate_visitor;
  RecordingHttp2Visitor vanilla_visitor;

  EXPECT_THAT(chocolate_visitor.GetEventSequence(), IsEmpty());
  EXPECT_THAT(vanilla_visitor.GetEventSequence(), IsEmpty());
  EXPECT_EQ(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());

  chocolate_visitor.OnSettingsStart();

  EXPECT_THAT(chocolate_visitor.GetEventSequence(), testing::Not(IsEmpty()));
  EXPECT_THAT(vanilla_visitor.GetEventSequence(), IsEmpty());
  EXPECT_NE(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());

  chocolate_visitor.Clear();

  EXPECT_THAT(chocolate_visitor.GetEventSequence(), IsEmpty());
  EXPECT_THAT(vanilla_visitor.GetEventSequence(), IsEmpty());
  EXPECT_EQ(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());
}

TEST(RecordingHttp2VisitorTest, SameEventsProduceSameSequence) {
  RecordingHttp2Visitor chocolate_visitor;
  RecordingHttp2Visitor vanilla_visitor;

  // Prepare some random values to deliver with the events.
  http2::test::Http2Random random;
  const Http2StreamId stream_id = random.Uniform(kMaxStreamId);
  const Http2StreamId another_stream_id = random.Uniform(kMaxStreamId);
  const size_t length = random.Rand16();
  const uint8_t type = random.Rand8();
  const uint8_t flags = random.Rand8();
  const Http2ErrorCode error_code = static_cast<Http2ErrorCode>(
      random.Uniform(static_cast<int>(Http2ErrorCode::MAX_ERROR_CODE)));
  const Http2Setting setting = {random.Rand16(), random.Rand32()};
  const absl::string_view alphabet = "abcdefghijklmnopqrstuvwxyz0123456789-";
  const std::string some_string =
      random.RandStringWithAlphabet(random.Rand8(), alphabet);
  const std::string another_string =
      random.RandStringWithAlphabet(random.Rand8(), alphabet);
  const uint16_t some_int = random.Rand16();
  const bool some_bool = random.OneIn(2);

  // Send the same arbitrary sequence of events to both visitors.
  std::list<RecordingHttp2Visitor*> visitors = {&chocolate_visitor,
                                                &vanilla_visitor};
  for (RecordingHttp2Visitor* visitor : visitors) {
    visitor->OnConnectionError(
        Http2VisitorInterface::ConnectionError::kSendError);
    visitor->OnFrameHeader(stream_id, length, type, flags);
    visitor->OnSettingsStart();
    visitor->OnSetting(setting);
    visitor->OnSettingsEnd();
    visitor->OnSettingsAck();
    visitor->OnBeginHeadersForStream(stream_id);
    visitor->OnHeaderForStream(stream_id, some_string, another_string);
    visitor->OnEndHeadersForStream(stream_id);
    visitor->OnBeginDataForStream(stream_id, length);
    visitor->OnDataForStream(stream_id, some_string);
    visitor->OnDataForStream(stream_id, another_string);
    visitor->OnEndStream(stream_id);
    visitor->OnRstStream(stream_id, error_code);
    visitor->OnCloseStream(stream_id, error_code);
    visitor->OnPriorityForStream(stream_id, another_stream_id, some_int,
                                 some_bool);
    visitor->OnPing(some_int, some_bool);
    visitor->OnPushPromiseForStream(stream_id, another_stream_id);
    visitor->OnGoAway(stream_id, error_code, some_string);
    visitor->OnWindowUpdate(stream_id, some_int);
    visitor->OnBeginMetadataForStream(stream_id, length);
    visitor->OnMetadataForStream(stream_id, some_string);
    visitor->OnMetadataForStream(stream_id, another_string);
    visitor->OnMetadataEndForStream(stream_id);
  }

  EXPECT_EQ(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());
}

TEST(RecordingHttp2VisitorTest, DifferentEventsProduceDifferentSequence) {
  RecordingHttp2Visitor chocolate_visitor;
  RecordingHttp2Visitor vanilla_visitor;
  EXPECT_EQ(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());

  const Http2StreamId stream_id = 1;
  const size_t length = 42;

  // Different events with the same method arguments should produce different
  // event sequences.
  chocolate_visitor.OnBeginDataForStream(stream_id, length);
  vanilla_visitor.OnBeginMetadataForStream(stream_id, length);
  EXPECT_NE(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());

  chocolate_visitor.Clear();
  vanilla_visitor.Clear();
  EXPECT_EQ(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());

  // The same events with different method arguments should produce different
  // event sequences.
  chocolate_visitor.OnBeginHeadersForStream(stream_id);
  vanilla_visitor.OnBeginHeadersForStream(stream_id + 2);
  EXPECT_NE(chocolate_visitor.GetEventSequence(),
            vanilla_visitor.GetEventSequence());
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
