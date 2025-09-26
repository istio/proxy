// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_stream_send_buffer.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_stream_send_buffer_base.h"
#include "quiche/quic/core/quic_stream_send_buffer_inlining.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/test_tools/quic_stream_send_buffer_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_endian.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/simple_buffer_allocator.h"

namespace quic {
namespace test {
namespace {

enum class SendBufferType {
  kDefault,
  kInlining,
};

std::string SendBufferTypeName(
    const testing::TestParamInfo<SendBufferType>& type) {
  switch (type.param) {
    case SendBufferType::kDefault:
      return "Default";
    case SendBufferType::kInlining:
      return "Inlining";
  }
  return "<invalid>";
}

class QuicStreamSendBufferTest
    : public quiche::test::QuicheTestWithParam<SendBufferType> {
 public:
  QuicStreamSendBufferTest() {
    send_buffer_ = CreateBuffer();
    EXPECT_EQ(0u, send_buffer_->size());
    EXPECT_EQ(0u, send_buffer_->stream_bytes_written());
    EXPECT_EQ(0u, send_buffer_->stream_bytes_outstanding());

    std::string data1 = absl::StrCat(
        std::string(1536, 'a'), std::string(256, 'b'), std::string(256, 'c'));

    quiche::QuicheBuffer buffer1(&allocator_, 1024);
    memset(buffer1.data(), 'c', buffer1.size());
    quiche::QuicheMemSlice slice1(std::move(buffer1));

    quiche::QuicheBuffer buffer2(&allocator_, 768);
    memset(buffer2.data(), 'd', buffer2.size());
    quiche::QuicheMemSlice slice2(std::move(buffer2));

    // `data` will be split into two BufferedSlices.
    SetQuicFlag(quic_send_buffer_max_data_slice_size, 1024);
    send_buffer_->SaveStreamData(data1);

    send_buffer_->SaveMemSlice(std::move(slice1));
    EXPECT_TRUE(slice1.empty());
    send_buffer_->SaveMemSlice(std::move(slice2));
    EXPECT_TRUE(slice2.empty());

    EXPECT_EQ(4u, send_buffer_->size());
    // At this point, `send_buffer_->interval_deque_` looks like this:
    // BufferedSlice1: 'a' * 1024
    // BufferedSlice2: 'a' * 512 + 'b' * 256 + 'c' * 256
    // BufferedSlice3: 'c' * 1024
    // BufferedSlice4: 'd' * 768
  }

  std::unique_ptr<QuicStreamSendBufferBase> CreateBuffer() {
    switch (GetParam()) {
      case SendBufferType::kDefault:
        return std::make_unique<QuicStreamSendBuffer>(&allocator_);
      case SendBufferType::kInlining:
        return std::make_unique<QuicStreamSendBufferInlining>(&allocator_);
    }
    return nullptr;
  }

  void WriteAllData() {
    // Write all data.
    char buf[4000];
    QuicDataWriter writer(4000, buf, quiche::HOST_BYTE_ORDER);
    EXPECT_TRUE(send_buffer_->WriteStreamData(0, 3840u, &writer));

    send_buffer_->OnStreamDataConsumed(3840u);
    EXPECT_EQ(3840u, send_buffer_->stream_bytes_written());
    EXPECT_EQ(3840u, send_buffer_->stream_bytes_outstanding());
  }

  quiche::SimpleBufferAllocator allocator_;
  std::unique_ptr<QuicStreamSendBufferBase> send_buffer_;
};

INSTANTIATE_TEST_SUITE_P(QuicStreamSendBufferTests, QuicStreamSendBufferTest,
                         testing::Values(SendBufferType::kDefault,
                                         SendBufferType::kInlining),
                         SendBufferTypeName);

TEST_P(QuicStreamSendBufferTest, CopyDataToBuffer) {
  char buf[4000];
  QuicDataWriter writer(4000, buf, quiche::HOST_BYTE_ORDER);
  std::string copy1(1024, 'a');
  std::string copy2 =
      std::string(512, 'a') + std::string(256, 'b') + std::string(256, 'c');
  std::string copy3(1024, 'c');
  std::string copy4(768, 'd');

  ASSERT_TRUE(send_buffer_->WriteStreamData(0, 1024, &writer));
  EXPECT_EQ(copy1, absl::string_view(buf, 1024));
  ASSERT_TRUE(send_buffer_->WriteStreamData(1024, 1024, &writer));
  EXPECT_EQ(copy2, absl::string_view(buf + 1024, 1024));
  ASSERT_TRUE(send_buffer_->WriteStreamData(2048, 1024, &writer));
  EXPECT_EQ(copy3, absl::string_view(buf + 2048, 1024));
  ASSERT_TRUE(send_buffer_->WriteStreamData(3072, 768, &writer));
  EXPECT_EQ(copy4, absl::string_view(buf + 3072, 768));

  // Test data piece across boundries.
  QuicDataWriter writer2(4000, buf, quiche::HOST_BYTE_ORDER);
  std::string copy5 =
      std::string(536, 'a') + std::string(256, 'b') + std::string(232, 'c');
  ASSERT_TRUE(send_buffer_->WriteStreamData(1000, 1024, &writer2));
  EXPECT_EQ(copy5, absl::string_view(buf, 1024));
  ASSERT_TRUE(send_buffer_->WriteStreamData(2500, 1024, &writer2));
  std::string copy6 = std::string(572, 'c') + std::string(452, 'd');
  EXPECT_EQ(copy6, absl::string_view(buf + 1024, 1024));

  // Invalid data copy.
  QuicDataWriter writer3(4000, buf, quiche::HOST_BYTE_ORDER);
  EXPECT_FALSE(send_buffer_->WriteStreamData(3000, 1024, &writer3));
  EXPECT_QUIC_BUG(send_buffer_->WriteStreamData(0, 4000, &writer3),
                  "Writer fails to write.");

  send_buffer_->OnStreamDataConsumed(3840);
  EXPECT_EQ(3840u, send_buffer_->stream_bytes_written());
  EXPECT_EQ(3840u, send_buffer_->stream_bytes_outstanding());
}

// Regression test for b/143491027.
TEST_P(QuicStreamSendBufferTest,
       WriteStreamDataContainsBothRetransmissionAndNewData) {
  std::string copy1(1024, 'a');
  std::string copy2 =
      std::string(512, 'a') + std::string(256, 'b') + std::string(256, 'c');
  std::string copy3 = std::string(1024, 'c') + std::string(100, 'd');
  char buf[6000];
  QuicDataWriter writer(6000, buf, quiche::HOST_BYTE_ORDER);
  // Write more than one slice.
  if (GetParam() == SendBufferType::kDefault) {
    EXPECT_EQ(0, QuicStreamSendBufferPeer::write_index(
                     static_cast<QuicStreamSendBuffer*>(send_buffer_.get())));
  }
  ASSERT_TRUE(send_buffer_->WriteStreamData(0, 1024, &writer));
  EXPECT_EQ(copy1, absl::string_view(buf, 1024));
  if (GetParam() == SendBufferType::kDefault) {
    EXPECT_EQ(1, QuicStreamSendBufferPeer::write_index(
                     static_cast<QuicStreamSendBuffer*>(send_buffer_.get())));
  }

  // Retransmit the first frame and also send new data.
  ASSERT_TRUE(send_buffer_->WriteStreamData(0, 2048, &writer));
  EXPECT_EQ(copy1 + copy2, absl::string_view(buf + 1024, 2048));

  // Write new data.
  ASSERT_TRUE(send_buffer_->WriteStreamData(2048, 50, &writer));
  EXPECT_EQ(std::string(50, 'c'), absl::string_view(buf + 1024 + 2048, 50));
  ASSERT_TRUE(send_buffer_->WriteStreamData(2048, 1124, &writer));
  EXPECT_EQ(copy3, absl::string_view(buf + 1024 + 2048 + 50, 1124));
}

TEST_P(QuicStreamSendBufferTest, RemoveStreamFrame) {
  WriteAllData();

  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(1024, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(2048, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(0, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);

  // Send buffer is cleaned up in order.
  EXPECT_EQ(1u, send_buffer_->size());
  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(3072, 768, &newly_acked_length));
  EXPECT_EQ(768u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_->size());
}

TEST_P(QuicStreamSendBufferTest, RemoveStreamFrameAcrossBoundaries) {
  WriteAllData();

  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(2024, 576, &newly_acked_length));
  EXPECT_EQ(576u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(0, 1000, &newly_acked_length));
  EXPECT_EQ(1000u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(1000, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  // Send buffer is cleaned up in order.
  EXPECT_EQ(2u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(2600, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  EXPECT_EQ(1u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(3624, 216, &newly_acked_length));
  EXPECT_EQ(216u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_->size());
}

TEST_P(QuicStreamSendBufferTest, AckStreamDataMultipleTimes) {
  WriteAllData();
  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(100, 1500, &newly_acked_length));
  EXPECT_EQ(1500u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(2000, 500, &newly_acked_length));
  EXPECT_EQ(500u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(0, 2600, &newly_acked_length));
  EXPECT_EQ(600u, newly_acked_length);
  // Send buffer is cleaned up in order.
  EXPECT_EQ(2u, send_buffer_->size());

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(2200, 1640, &newly_acked_length));
  EXPECT_EQ(1240u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_->size());

  EXPECT_FALSE(send_buffer_->OnStreamDataAcked(4000, 100, &newly_acked_length));
}

TEST_P(QuicStreamSendBufferTest, AckStreamDataOutOfOrder) {
  WriteAllData();
  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(500, 1000, &newly_acked_length));
  EXPECT_EQ(1000u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());
  EXPECT_EQ(3840u, QuicStreamSendBufferPeer::TotalLength(send_buffer_.get()));

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(1200, 1000, &newly_acked_length));
  EXPECT_EQ(700u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());
  // Slice 2 gets fully acked.
  EXPECT_EQ(2816u, QuicStreamSendBufferPeer::TotalLength(send_buffer_.get()));

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(2000, 1840, &newly_acked_length));
  EXPECT_EQ(1640u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_->size());
  // Slices 3 and 4 get fully acked.
  EXPECT_EQ(1024u, QuicStreamSendBufferPeer::TotalLength(send_buffer_.get()));

  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(0, 1000, &newly_acked_length));
  EXPECT_EQ(500u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_->size());
  EXPECT_EQ(0u, QuicStreamSendBufferPeer::TotalLength(send_buffer_.get()));
}

TEST_P(QuicStreamSendBufferTest, PendingRetransmission) {
  WriteAllData();
  EXPECT_TRUE(send_buffer_->IsStreamDataOutstanding(0, 3840));
  EXPECT_FALSE(send_buffer_->HasPendingRetransmission());
  // Lost data [0, 1200).
  send_buffer_->OnStreamDataLost(0, 1200);
  // Lost data [1500, 2000).
  send_buffer_->OnStreamDataLost(1500, 500);
  EXPECT_TRUE(send_buffer_->HasPendingRetransmission());

  EXPECT_EQ(StreamPendingRetransmission(0, 1200),
            send_buffer_->NextPendingRetransmission());
  // Retransmit data [0, 500).
  send_buffer_->OnStreamDataRetransmitted(0, 500);
  EXPECT_TRUE(send_buffer_->IsStreamDataOutstanding(0, 500));
  EXPECT_EQ(StreamPendingRetransmission(500, 700),
            send_buffer_->NextPendingRetransmission());
  // Ack data [500, 1200).
  QuicByteCount newly_acked_length = 0;
  EXPECT_TRUE(send_buffer_->OnStreamDataAcked(500, 700, &newly_acked_length));
  EXPECT_FALSE(send_buffer_->IsStreamDataOutstanding(500, 700));
  EXPECT_TRUE(send_buffer_->HasPendingRetransmission());
  EXPECT_EQ(StreamPendingRetransmission(1500, 500),
            send_buffer_->NextPendingRetransmission());
  // Retransmit data [1500, 2000).
  send_buffer_->OnStreamDataRetransmitted(1500, 500);
  EXPECT_FALSE(send_buffer_->HasPendingRetransmission());

  // Lost [200, 800).
  send_buffer_->OnStreamDataLost(200, 600);
  EXPECT_TRUE(send_buffer_->HasPendingRetransmission());
  // Verify [200, 500) is considered as lost, as [500, 800) has been acked.
  EXPECT_EQ(StreamPendingRetransmission(200, 300),
            send_buffer_->NextPendingRetransmission());

  // Verify 0 length data is not outstanding.
  EXPECT_FALSE(send_buffer_->IsStreamDataOutstanding(100, 0));
  // Verify partially acked data is outstanding.
  EXPECT_TRUE(send_buffer_->IsStreamDataOutstanding(400, 800));
}

TEST_P(QuicStreamSendBufferTest, OutOfOrderWrites) {
  char buf[3840] = {};
  // Write data out of order.
  QuicDataWriter writer2(sizeof(buf) - 1000, buf + 1000);
  EXPECT_TRUE(send_buffer_->WriteStreamData(1000u, 1000u, &writer2));
  QuicDataWriter writer4(sizeof(buf) - 3000, buf + 3000);
  EXPECT_TRUE(send_buffer_->WriteStreamData(3000u, 840u, &writer4));
  QuicDataWriter writer3(sizeof(buf) - 2000, buf + 2000);
  EXPECT_TRUE(send_buffer_->WriteStreamData(2000u, 1000u, &writer3));
  QuicDataWriter writer1(sizeof(buf), buf);
  EXPECT_TRUE(send_buffer_->WriteStreamData(0u, 1000u, &writer1));
  // Make sure it is correct.
  EXPECT_EQ(absl::string_view(buf, sizeof(buf)),
            absl::StrCat(std::string(1536, 'a'), std::string(256, 'b'),
                         std::string(1280, 'c'), std::string(768, 'd')));
}

TEST_P(QuicStreamSendBufferTest, SaveMemSliceSpan) {
  std::unique_ptr<QuicStreamSendBufferBase> send_buffer = CreateBuffer();

  std::string data(1024, 'a');
  std::vector<quiche::QuicheMemSlice> buffers;
  for (size_t i = 0; i < 10; ++i) {
    buffers.push_back(MemSliceFromString(data));
  }

  EXPECT_EQ(10 * 1024u, send_buffer->SaveMemSliceSpan(absl::MakeSpan(buffers)));
  EXPECT_EQ(10u, send_buffer->size());
}

TEST_P(QuicStreamSendBufferTest, SaveEmptyMemSliceSpan) {
  std::unique_ptr<QuicStreamSendBufferBase> send_buffer = CreateBuffer();

  std::string data(1024, 'a');
  std::vector<quiche::QuicheMemSlice> buffers;
  for (size_t i = 0; i < 10; ++i) {
    buffers.push_back(MemSliceFromString(data));
  }

  EXPECT_EQ(10 * 1024u, send_buffer->SaveMemSliceSpan(absl::MakeSpan(buffers)));
  // Verify the empty slice does not get saved.
  EXPECT_EQ(10u, send_buffer->size());
}

TEST_P(QuicStreamSendBufferTest, SmallWrite) {
  std::unique_ptr<QuicStreamSendBufferBase> send_buffer = CreateBuffer();

  constexpr absl::string_view kData = "abcd";
  send_buffer->SaveStreamData(kData);
  EXPECT_EQ(1u, send_buffer->size());
  EXPECT_EQ(4u, send_buffer->TotalDataBufferedForTest());

  char buffer[16];
  QuicDataWriter writer(sizeof(buffer), buffer);
  ASSERT_TRUE(send_buffer->WriteStreamData(0, 4, &writer));
  EXPECT_EQ(absl::string_view(buffer, 4), kData);
}

}  // namespace
}  // namespace test
}  // namespace quic
