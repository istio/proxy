#include "quiche/http2/adapter/nghttp2_data_provider.h"

#include "quiche/http2/adapter/nghttp2_util.h"
#include "quiche/http2/adapter/test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {

const size_t kFrameHeaderSize = 9;

// Verifies that the Visitor read callback works correctly when the amount of
// data read is less than what the source provides.
TEST(VisitorTest, ReadLessThanSourceProvides) {
  const int32_t kStreamId = 1;
  TestVisitor visitor;
  visitor.AppendPayloadForStream(kStreamId, "Example payload");
  visitor.SetEndData(kStreamId, true);
  uint32_t data_flags = 0;
  const size_t kReadLength = 10;
  // Read callback selects a payload length given an upper bound.
  ssize_t result = callbacks::VisitorReadCallback(visitor, kStreamId,
                                                  kReadLength, &data_flags);
  ASSERT_EQ(kReadLength, result);
  EXPECT_EQ(NGHTTP2_DATA_FLAG_NO_COPY | NGHTTP2_DATA_FLAG_NO_END_STREAM,
            data_flags);

  const uint8_t framehd[kFrameHeaderSize] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  // Sends the frame header and some payload bytes.
  visitor.SendDataFrame(kStreamId, ToStringView(framehd, kFrameHeaderSize),
                        result);
  // Data accepted by the visitor includes a frame header and kReadLength bytes
  // of payload.
  EXPECT_EQ(visitor.data().size(), kFrameHeaderSize + kReadLength);
}

// Verifies that the Visitor read callback works correctly when the amount of
// data read is more than what the source provides.
TEST(VisitorTest, ReadMoreThanSourceProvides) {
  const int32_t kStreamId = 1;
  const absl::string_view kPayload = "Example payload";
  TestVisitor visitor;
  visitor.AppendPayloadForStream(kStreamId, kPayload);
  visitor.SetEndData(kStreamId, true);
  uint32_t data_flags = 0;
  const size_t kReadLength = 30;
  // Read callback selects a payload length given an upper bound.
  ssize_t result = callbacks::VisitorReadCallback(visitor, kStreamId,
                                                  kReadLength, &data_flags);
  ASSERT_EQ(kPayload.size(), result);
  EXPECT_EQ(NGHTTP2_DATA_FLAG_NO_COPY | NGHTTP2_DATA_FLAG_EOF, data_flags);

  const uint8_t framehd[kFrameHeaderSize] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  // Sends the frame header and some payload bytes.
  visitor.SendDataFrame(kStreamId, ToStringView(framehd, kFrameHeaderSize),
                        result);
  // Data accepted by the visitor includes a frame header and the entire
  // payload.
  EXPECT_EQ(visitor.data().size(), kFrameHeaderSize + kPayload.size());
}

// Verifies that the Visitor read callback works correctly when the source is
// blocked.
TEST(VisitorTest, ReadFromBlockedSource) {
  const int32_t kStreamId = 1;
  TestVisitor visitor;
  // Stream has no payload, but also no fin, so it's blocked.
  uint32_t data_flags = 0;
  const size_t kReadLength = 10;
  ssize_t result = callbacks::VisitorReadCallback(visitor, kStreamId,
                                                  kReadLength, &data_flags);
  // Read operation is deferred, since the source is blocked.
  EXPECT_EQ(NGHTTP2_ERR_DEFERRED, result);
}

// Verifies that the Visitor read callback works correctly when the source
// provides only fin and no data.
TEST(VisitorTest, ReadFromZeroLengthSource) {
  const int32_t kStreamId = 1;
  TestVisitor visitor;
  // Empty payload and fin=true indicates the source is done.
  visitor.SetEndData(kStreamId, true);
  uint32_t data_flags = 0;
  const size_t kReadLength = 10;
  ssize_t result = callbacks::VisitorReadCallback(visitor, kStreamId,
                                                  kReadLength, &data_flags);
  ASSERT_EQ(0, result);
  EXPECT_EQ(NGHTTP2_DATA_FLAG_NO_COPY | NGHTTP2_DATA_FLAG_EOF, data_flags);

  const uint8_t framehd[kFrameHeaderSize] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  visitor.SendDataFrame(kStreamId, ToStringView(framehd, kFrameHeaderSize),
                        result);
  // Data accepted by the visitor includes a frame header with fin and zero
  // bytes of payload.
  EXPECT_EQ(visitor.data().size(), kFrameHeaderSize);
}

}  // namespace test
}  // namespace adapter
}  // namespace http2
