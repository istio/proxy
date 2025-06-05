// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_stream_sequencer_buffer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_stream_sequencer_buffer_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {

namespace test {

absl::string_view IovecToStringPiece(iovec iov) {
  return absl::string_view(reinterpret_cast<const char*>(iov.iov_base),
                           iov.iov_len);
}

char GetCharFromIOVecs(size_t offset, iovec iov[], size_t count) {
  size_t start_offset = 0;
  for (size_t i = 0; i < count; i++) {
    if (iov[i].iov_len == 0) {
      continue;
    }
    size_t end_offset = start_offset + iov[i].iov_len - 1;
    if (offset >= start_offset && offset <= end_offset) {
      const char* buf = reinterpret_cast<const char*>(iov[i].iov_base);
      return buf[offset - start_offset];
    }
    start_offset += iov[i].iov_len;
  }
  QUIC_LOG(ERROR) << "Could not locate char at offset " << offset << " in "
                  << count << " iovecs";
  for (size_t i = 0; i < count; ++i) {
    QUIC_LOG(ERROR) << "  iov[" << i << "].iov_len = " << iov[i].iov_len;
  }
  return '\0';
}

const size_t kMaxNumGapsAllowed = 2 * kMaxPacketGap;

static const size_t kBlockSizeBytes =
    QuicStreamSequencerBuffer::kBlockSizeBytes;
using BufferBlock = QuicStreamSequencerBuffer::BufferBlock;

namespace {

class QuicStreamSequencerBufferTest : public QuicTest {
 public:
  void SetUp() override { Initialize(); }

  void ResetMaxCapacityBytes(size_t max_capacity_bytes) {
    max_capacity_bytes_ = max_capacity_bytes;
    Initialize();
  }

 protected:
  void Initialize() {
    buffer_ =
        std::make_unique<QuicStreamSequencerBuffer>((max_capacity_bytes_));
    helper_ = std::make_unique<QuicStreamSequencerBufferPeer>((buffer_.get()));
  }

  // Use 8.5 here to make sure that the buffer has more than
  // QuicStreamSequencerBuffer::kInitialBlockCount block and its end doesn't
  // align with the end of a block in order to test all the offset calculation.
  size_t max_capacity_bytes_ = 8.5 * kBlockSizeBytes;

  std::unique_ptr<QuicStreamSequencerBuffer> buffer_;
  std::unique_ptr<QuicStreamSequencerBufferPeer> helper_;
  size_t written_ = 0;
  std::string error_details_;
};

TEST_F(QuicStreamSequencerBufferTest, InitializeWithMaxRecvWindowSize) {
  ResetMaxCapacityBytes(16 * 1024 * 1024);  // 16MB
  EXPECT_EQ(2 * 1024u,                      // 16MB / 8KB = 2K
            helper_->max_blocks_count());
  EXPECT_EQ(max_capacity_bytes_, helper_->max_buffer_capacity());
  EXPECT_TRUE(helper_->CheckInitialState());
}

TEST_F(QuicStreamSequencerBufferTest, InitializationWithDifferentSizes) {
  const size_t kCapacity = 16 * QuicStreamSequencerBuffer::kBlockSizeBytes;
  ResetMaxCapacityBytes(kCapacity);
  EXPECT_EQ(max_capacity_bytes_, helper_->max_buffer_capacity());
  EXPECT_TRUE(helper_->CheckInitialState());

  const size_t kCapacity1 = 32 * QuicStreamSequencerBuffer::kBlockSizeBytes;
  ResetMaxCapacityBytes(kCapacity1);
  EXPECT_EQ(kCapacity1, helper_->max_buffer_capacity());
  EXPECT_TRUE(helper_->CheckInitialState());
}

TEST_F(QuicStreamSequencerBufferTest, ClearOnEmpty) {
  buffer_->Clear();
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamData0length) {
  QuicErrorCode error =
      buffer_->OnStreamData(800, "", &written_, &error_details_);
  EXPECT_THAT(error, IsError(QUIC_EMPTY_STREAM_FRAME_NO_FIN));
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataWithinBlock) {
  EXPECT_FALSE(helper_->IsBufferAllocated());
  std::string source(1024, 'a');
  EXPECT_THAT(buffer_->OnStreamData(800, source, &written_, &error_details_),
              IsQuicNoError());
  BufferBlock* block_ptr = helper_->GetBlock(0);
  for (size_t i = 0; i < source.size(); ++i) {
    ASSERT_EQ('a', block_ptr->buffer[helper_->GetInBlockOffset(800) + i]);
  }
  EXPECT_EQ(2, helper_->IntervalSize());
  EXPECT_EQ(0u, helper_->ReadableBytes());
  EXPECT_EQ(1u, helper_->bytes_received().Size());
  EXPECT_EQ(800u, helper_->bytes_received().begin()->min());
  EXPECT_EQ(1824u, helper_->bytes_received().begin()->max());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
  EXPECT_TRUE(helper_->IsBufferAllocated());
}

TEST_F(QuicStreamSequencerBufferTest, Move) {
  EXPECT_FALSE(helper_->IsBufferAllocated());
  std::string source(1024, 'a');
  EXPECT_THAT(buffer_->OnStreamData(800, source, &written_, &error_details_),
              IsQuicNoError());
  BufferBlock* block_ptr = helper_->GetBlock(0);
  for (size_t i = 0; i < source.size(); ++i) {
    ASSERT_EQ('a', block_ptr->buffer[helper_->GetInBlockOffset(800) + i]);
  }

  QuicStreamSequencerBuffer buffer2(std::move(*buffer_));
  QuicStreamSequencerBufferPeer helper2(&buffer2);

  EXPECT_FALSE(helper_->IsBufferAllocated());

  EXPECT_EQ(2, helper2.IntervalSize());
  EXPECT_EQ(0u, helper2.ReadableBytes());
  EXPECT_EQ(1u, helper2.bytes_received().Size());
  EXPECT_EQ(800u, helper2.bytes_received().begin()->min());
  EXPECT_EQ(1824u, helper2.bytes_received().begin()->max());
  EXPECT_TRUE(helper2.CheckBufferInvariants());
  EXPECT_TRUE(helper2.IsBufferAllocated());
}

TEST_F(QuicStreamSequencerBufferTest, DISABLED_OnStreamDataInvalidSource) {
  // Pass in an invalid source, expects to return error.
  absl::string_view source;
  source = absl::string_view(nullptr, 1024);
  EXPECT_THAT(buffer_->OnStreamData(800, source, &written_, &error_details_),
              IsError(QUIC_STREAM_SEQUENCER_INVALID_STATE));
  EXPECT_EQ(0u, error_details_.find(absl::StrCat(
                    "QuicStreamSequencerBuffer error: OnStreamData() "
                    "dest == nullptr: ",
                    false, " source == nullptr: ", true)));
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataWithOverlap) {
  std::string source(1024, 'a');
  // Write something into [800, 1824)
  EXPECT_THAT(buffer_->OnStreamData(800, source, &written_, &error_details_),
              IsQuicNoError());
  // Try to write to [0, 1024) and [1024, 2048).
  EXPECT_THAT(buffer_->OnStreamData(0, source, &written_, &error_details_),
              IsQuicNoError());
  EXPECT_THAT(buffer_->OnStreamData(1024, source, &written_, &error_details_),
              IsQuicNoError());
}

TEST_F(QuicStreamSequencerBufferTest,
       OnStreamDataOverlapAndDuplicateCornerCases) {
  std::string source(1024, 'a');
  // Write something into [800, 1824)
  buffer_->OnStreamData(800, source, &written_, &error_details_);
  source = std::string(800, 'b');
  std::string one_byte = "c";
  // Write [1, 801).
  EXPECT_THAT(buffer_->OnStreamData(1, source, &written_, &error_details_),
              IsQuicNoError());
  // Write [0, 800).
  EXPECT_THAT(buffer_->OnStreamData(0, source, &written_, &error_details_),
              IsQuicNoError());
  // Write [1823, 1824).
  EXPECT_THAT(buffer_->OnStreamData(1823, one_byte, &written_, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(0u, written_);
  // write one byte to [1824, 1825)
  EXPECT_THAT(buffer_->OnStreamData(1824, one_byte, &written_, &error_details_),
              IsQuicNoError());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataWithoutOverlap) {
  std::string source(1024, 'a');
  // Write something into [800, 1824).
  EXPECT_THAT(buffer_->OnStreamData(800, source, &written_, &error_details_),
              IsQuicNoError());
  source = std::string(100, 'b');
  // Write something into [kBlockSizeBytes * 2 - 20, kBlockSizeBytes * 2 + 80).
  EXPECT_THAT(buffer_->OnStreamData(kBlockSizeBytes * 2 - 20, source, &written_,
                                    &error_details_),
              IsQuicNoError());
  EXPECT_EQ(3, helper_->IntervalSize());
  EXPECT_EQ(1024u + 100u, buffer_->BytesBuffered());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataInLongStreamWithOverlap) {
  // Assume a stream has already buffered almost 4GB.
  uint64_t total_bytes_read = pow(2, 32) - 1;
  helper_->set_total_bytes_read(total_bytes_read);
  helper_->AddBytesReceived(0, total_bytes_read);

  // Three new out of order frames arrive.
  const size_t kBytesToWrite = 100;
  std::string source(kBytesToWrite, 'a');
  // Frame [2^32 + 500, 2^32 + 600).
  QuicStreamOffset offset = pow(2, 32) + 500;
  EXPECT_THAT(buffer_->OnStreamData(offset, source, &written_, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(2, helper_->IntervalSize());

  // Frame [2^32 + 700, 2^32 + 800).
  offset = pow(2, 32) + 700;
  EXPECT_THAT(buffer_->OnStreamData(offset, source, &written_, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(3, helper_->IntervalSize());

  // Another frame [2^32 + 300, 2^32 + 400).
  offset = pow(2, 32) + 300;
  EXPECT_THAT(buffer_->OnStreamData(offset, source, &written_, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(4, helper_->IntervalSize());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataTillEnd) {
  // Write 50 bytes to the end.
  const size_t kBytesToWrite = 50;
  std::string source(kBytesToWrite, 'a');
  EXPECT_THAT(buffer_->OnStreamData(max_capacity_bytes_ - kBytesToWrite, source,
                                    &written_, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(50u, buffer_->BytesBuffered());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataTillEndCorner) {
  // Write 1 byte to the end.
  const size_t kBytesToWrite = 1;
  std::string source(kBytesToWrite, 'a');
  EXPECT_THAT(buffer_->OnStreamData(max_capacity_bytes_ - kBytesToWrite, source,
                                    &written_, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(1u, buffer_->BytesBuffered());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, OnStreamDataBeyondCapacity) {
  std::string source(60, 'a');
  EXPECT_THAT(buffer_->OnStreamData(max_capacity_bytes_ - 50, source, &written_,
                                    &error_details_),
              IsError(QUIC_INTERNAL_ERROR));
  EXPECT_TRUE(helper_->CheckBufferInvariants());

  source = "b";
  EXPECT_THAT(buffer_->OnStreamData(max_capacity_bytes_, source, &written_,
                                    &error_details_),
              IsError(QUIC_INTERNAL_ERROR));
  EXPECT_TRUE(helper_->CheckBufferInvariants());

  EXPECT_THAT(buffer_->OnStreamData(max_capacity_bytes_ * 1000, source,
                                    &written_, &error_details_),
              IsError(QUIC_INTERNAL_ERROR));
  EXPECT_TRUE(helper_->CheckBufferInvariants());

  // Disallow current_gap != gaps_.end()
  EXPECT_THAT(buffer_->OnStreamData(static_cast<QuicStreamOffset>(-1), source,
                                    &written_, &error_details_),
              IsError(QUIC_INTERNAL_ERROR));
  EXPECT_TRUE(helper_->CheckBufferInvariants());

  // Disallow offset + size overflow
  source = "bbb";
  EXPECT_THAT(buffer_->OnStreamData(static_cast<QuicStreamOffset>(-2), source,
                                    &written_, &error_details_),
              IsError(QUIC_INTERNAL_ERROR));
  EXPECT_TRUE(helper_->CheckBufferInvariants());
  EXPECT_EQ(0u, buffer_->BytesBuffered());
}

TEST_F(QuicStreamSequencerBufferTest, Readv100Bytes) {
  std::string source(1024, 'a');
  // Write something into [kBlockSizeBytes, kBlockSizeBytes + 1024).
  buffer_->OnStreamData(kBlockSizeBytes, source, &written_, &error_details_);
  EXPECT_FALSE(buffer_->HasBytesToRead());
  source = std::string(100, 'b');
  // Write something into [0, 100).
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  EXPECT_TRUE(buffer_->HasBytesToRead());
  // Read into a iovec array with total capacity of 120 bytes.
  char dest[120];
  iovec iovecs[3]{iovec{dest, 40}, iovec{dest + 40, 40}, iovec{dest + 80, 40}};
  size_t read;
  EXPECT_THAT(buffer_->Readv(iovecs, 3, &read, &error_details_),
              IsQuicNoError());
  QUIC_LOG(ERROR) << error_details_;
  EXPECT_EQ(100u, read);
  EXPECT_EQ(100u, buffer_->BytesConsumed());
  EXPECT_EQ(source, absl::string_view(dest, read));
  // The first block should be released as its data has been read out.
  EXPECT_EQ(nullptr, helper_->GetBlock(0));
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, ReadvAcrossBlocks) {
  std::string source(kBlockSizeBytes + 50, 'a');
  // Write 1st block to full and extand 50 bytes to next block.
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  EXPECT_EQ(source.size(), helper_->ReadableBytes());
  // Iteratively read 512 bytes from buffer_-> Overwrite dest[] each time.
  char dest[512];
  while (helper_->ReadableBytes()) {
    std::fill(dest, dest + 512, 0);
    iovec iovecs[2]{iovec{dest, 256}, iovec{dest + 256, 256}};
    size_t read;
    EXPECT_THAT(buffer_->Readv(iovecs, 2, &read, &error_details_),
                IsQuicNoError());
  }
  // The last read only reads the rest 50 bytes in 2nd block.
  EXPECT_EQ(std::string(50, 'a'), std::string(dest, 50));
  EXPECT_EQ(0, dest[50]) << "Dest[50] shouln't be filled.";
  EXPECT_EQ(source.size(), buffer_->BytesConsumed());
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, ClearAfterRead) {
  std::string source(kBlockSizeBytes + 50, 'a');
  // Write 1st block to full with 'a'.
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  // Read first 512 bytes from buffer to make space at the beginning.
  char dest[512]{0};
  const iovec iov{dest, 512};
  size_t read;
  EXPECT_THAT(buffer_->Readv(&iov, 1, &read, &error_details_), IsQuicNoError());
  // Clear() should make buffer empty while preserving BytesConsumed()
  buffer_->Clear();
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest,
       OnStreamDataAcrossLastBlockAndFillCapacity) {
  std::string source(kBlockSizeBytes + 50, 'a');
  // Write 1st block to full with 'a'.
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  // Read first 512 bytes from buffer to make space at the beginning.
  char dest[512]{0};
  const iovec iov{dest, 512};
  size_t read;
  EXPECT_THAT(buffer_->Readv(&iov, 1, &read, &error_details_), IsQuicNoError());
  EXPECT_EQ(source.size(), written_);

  // Write more than half block size of bytes in the last block with 'b', which
  // will wrap to the beginning and reaches the full capacity.
  source = std::string(0.5 * kBlockSizeBytes + 512, 'b');
  EXPECT_THAT(buffer_->OnStreamData(2 * kBlockSizeBytes, source, &written_,
                                    &error_details_),
              IsQuicNoError());
  EXPECT_EQ(source.size(), written_);
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest,
       OnStreamDataAcrossLastBlockAndExceedCapacity) {
  std::string source(kBlockSizeBytes + 50, 'a');
  // Write 1st block to full.
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  // Read first 512 bytes from buffer to make space at the beginning.
  char dest[512]{0};
  const iovec iov{dest, 512};
  size_t read;
  EXPECT_THAT(buffer_->Readv(&iov, 1, &read, &error_details_), IsQuicNoError());

  // Try to write from [max_capacity_bytes_ - 0.5 * kBlockSizeBytes,
  // max_capacity_bytes_ +  512 + 1). But last bytes exceeds current capacity.
  source = std::string(0.5 * kBlockSizeBytes + 512 + 1, 'b');
  EXPECT_THAT(buffer_->OnStreamData(8 * kBlockSizeBytes, source, &written_,
                                    &error_details_),
              IsError(QUIC_INTERNAL_ERROR));
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, ReadvAcrossLastBlock) {
  // Write to full capacity and read out 512 bytes at beginning and continue
  // appending 256 bytes.
  std::string source(max_capacity_bytes_, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[512]{0};
  const iovec iov{dest, 512};
  size_t read;
  EXPECT_THAT(buffer_->Readv(&iov, 1, &read, &error_details_), IsQuicNoError());
  source = std::string(256, 'b');
  buffer_->OnStreamData(max_capacity_bytes_, source, &written_,
                        &error_details_);
  EXPECT_TRUE(helper_->CheckBufferInvariants());

  // Read all data out.
  std::unique_ptr<char[]> dest1{new char[max_capacity_bytes_]};
  dest1[0] = 0;
  const iovec iov1{dest1.get(), max_capacity_bytes_};
  EXPECT_THAT(buffer_->Readv(&iov1, 1, &read, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(max_capacity_bytes_ - 512 + 256, read);
  EXPECT_EQ(max_capacity_bytes_ + 256, buffer_->BytesConsumed());
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, ReadvEmpty) {
  char dest[512]{0};
  iovec iov{dest, 512};
  size_t read;
  EXPECT_THAT(buffer_->Readv(&iov, 1, &read, &error_details_), IsQuicNoError());
  EXPECT_EQ(0u, read);
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionsEmpty) {
  iovec iovs[2];
  int iov_count = buffer_->GetReadableRegions(iovs, 2);
  EXPECT_EQ(0, iov_count);
  EXPECT_EQ(nullptr, iovs[iov_count].iov_base);
  EXPECT_EQ(0u, iovs[iov_count].iov_len);
}

TEST_F(QuicStreamSequencerBufferTest, ReleaseWholeBuffer) {
  // Tests that buffer is not deallocated unless ReleaseWholeBuffer() is called.
  std::string source(100, 'b');
  // Write something into [0, 100).
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  EXPECT_TRUE(buffer_->HasBytesToRead());
  char dest[120];
  iovec iovecs[3]{iovec{dest, 40}, iovec{dest + 40, 40}, iovec{dest + 80, 40}};
  size_t read;
  EXPECT_THAT(buffer_->Readv(iovecs, 3, &read, &error_details_),
              IsQuicNoError());
  EXPECT_EQ(100u, read);
  EXPECT_EQ(100u, buffer_->BytesConsumed());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
  EXPECT_TRUE(helper_->IsBufferAllocated());
  buffer_->ReleaseWholeBuffer();
  EXPECT_FALSE(helper_->IsBufferAllocated());
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionsBlockedByGap) {
  // Write into [1, 1024).
  std::string source(1023, 'a');
  buffer_->OnStreamData(1, source, &written_, &error_details_);
  // Try to get readable regions, but none is there.
  iovec iovs[2];
  int iov_count = buffer_->GetReadableRegions(iovs, 2);
  EXPECT_EQ(0, iov_count);
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionsTillEndOfBlock) {
  // Write first block to full with [0, 256) 'a' and the rest 'b' then read out
  // [0, 256)
  std::string source(kBlockSizeBytes, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[256];
  helper_->Read(dest, 256);
  // Get readable region from [256, 1024)
  iovec iovs[2];
  int iov_count = buffer_->GetReadableRegions(iovs, 2);
  EXPECT_EQ(1, iov_count);
  EXPECT_EQ(std::string(kBlockSizeBytes - 256, 'a'),
            IovecToStringPiece(iovs[0]));
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionsWithinOneBlock) {
  // Write into [0, 1024) and then read out [0, 256)
  std::string source(1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[256];
  helper_->Read(dest, 256);
  // Get readable region from [256, 1024)
  iovec iovs[2];
  int iov_count = buffer_->GetReadableRegions(iovs, 2);
  EXPECT_EQ(1, iov_count);
  EXPECT_EQ(std::string(1024 - 256, 'a'), IovecToStringPiece(iovs[0]));
}

TEST_F(QuicStreamSequencerBufferTest,
       GetReadableRegionsAcrossBlockWithLongIOV) {
  // Write into [0, 2 * kBlockSizeBytes + 1024) and then read out [0, 1024)
  std::string source(2 * kBlockSizeBytes + 1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[1024];
  helper_->Read(dest, 1024);

  iovec iovs[4];
  int iov_count = buffer_->GetReadableRegions(iovs, 4);
  EXPECT_EQ(3, iov_count);
  EXPECT_EQ(kBlockSizeBytes - 1024, iovs[0].iov_len);
  EXPECT_EQ(kBlockSizeBytes, iovs[1].iov_len);
  EXPECT_EQ(1024u, iovs[2].iov_len);
}

TEST_F(QuicStreamSequencerBufferTest,
       GetReadableRegionsWithMultipleIOVsAcrossEnd) {
  // Write into [0, 8.5 * kBlockSizeBytes - 1024) and then read out [0, 1024)
  // and then append 1024 + 512 bytes.
  std::string source(8.5 * kBlockSizeBytes - 1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[1024];
  helper_->Read(dest, 1024);
  // Write across the end.
  source = std::string(1024 + 512, 'b');
  buffer_->OnStreamData(8.5 * kBlockSizeBytes - 1024, source, &written_,
                        &error_details_);
  // Use short iovec's.
  iovec iovs[2];
  int iov_count = buffer_->GetReadableRegions(iovs, 2);
  EXPECT_EQ(2, iov_count);
  EXPECT_EQ(kBlockSizeBytes - 1024, iovs[0].iov_len);
  EXPECT_EQ(kBlockSizeBytes, iovs[1].iov_len);
  // Use long iovec's and wrap the end of buffer.
  iovec iovs1[11];
  EXPECT_EQ(10, buffer_->GetReadableRegions(iovs1, 11));
  EXPECT_EQ(0.5 * kBlockSizeBytes, iovs1[8].iov_len);
  EXPECT_EQ(512u, iovs1[9].iov_len);
  EXPECT_EQ(std::string(512, 'b'), IovecToStringPiece(iovs1[9]));
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionEmpty) {
  iovec iov;
  EXPECT_FALSE(buffer_->GetReadableRegion(&iov));
  EXPECT_EQ(nullptr, iov.iov_base);
  EXPECT_EQ(0u, iov.iov_len);
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionBeforeGap) {
  // Write into [1, 1024).
  std::string source(1023, 'a');
  buffer_->OnStreamData(1, source, &written_, &error_details_);
  // GetReadableRegion should return false because range  [0,1) hasn't been
  // filled yet.
  iovec iov;
  EXPECT_FALSE(buffer_->GetReadableRegion(&iov));
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionTillEndOfBlock) {
  // Write into [0, kBlockSizeBytes + 1) and then read out [0, 256)
  std::string source(kBlockSizeBytes + 1, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[256];
  helper_->Read(dest, 256);
  // Get readable region from [256, 1024)
  iovec iov;
  EXPECT_TRUE(buffer_->GetReadableRegion(&iov));
  EXPECT_EQ(std::string(kBlockSizeBytes - 256, 'a'), IovecToStringPiece(iov));
}

TEST_F(QuicStreamSequencerBufferTest, GetReadableRegionTillGap) {
  // Write into [0, kBlockSizeBytes - 1) and then read out [0, 256)
  std::string source(kBlockSizeBytes - 1, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[256];
  helper_->Read(dest, 256);
  // Get readable region from [256, 1023)
  iovec iov;
  EXPECT_TRUE(buffer_->GetReadableRegion(&iov));
  EXPECT_EQ(std::string(kBlockSizeBytes - 1 - 256, 'a'),
            IovecToStringPiece(iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekEmptyBuffer) {
  iovec iov;
  EXPECT_FALSE(buffer_->PeekRegion(0, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(1, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(100, &iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekSingleBlock) {
  std::string source(kBlockSizeBytes, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);

  iovec iov;
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source, IovecToStringPiece(iov));

  // Peeking again gives the same result.
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source, IovecToStringPiece(iov));

  // Peek at a different offset.
  EXPECT_TRUE(buffer_->PeekRegion(100, &iov));
  EXPECT_EQ(absl::string_view(source).substr(100), IovecToStringPiece(iov));

  // Peeking at or after FirstMissingByte() returns false.
  EXPECT_FALSE(buffer_->PeekRegion(kBlockSizeBytes, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(kBlockSizeBytes + 1, &iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekTwoWritesInSingleBlock) {
  const size_t length1 = 1024;
  std::string source1(length1, 'a');
  buffer_->OnStreamData(0, source1, &written_, &error_details_);

  iovec iov;
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source1, IovecToStringPiece(iov));

  // The second frame goes into the same block.
  const size_t length2 = 800;
  std::string source2(length2, 'b');
  buffer_->OnStreamData(length1, source2, &written_, &error_details_);

  EXPECT_TRUE(buffer_->PeekRegion(length1, &iov));
  EXPECT_EQ(source2, IovecToStringPiece(iov));

  // Peek with an offset inside the first write.
  const QuicStreamOffset offset1 = 500;
  EXPECT_TRUE(buffer_->PeekRegion(offset1, &iov));
  EXPECT_EQ(absl::string_view(source1).substr(offset1),
            IovecToStringPiece(iov).substr(0, length1 - offset1));
  EXPECT_EQ(absl::string_view(source2),
            IovecToStringPiece(iov).substr(length1 - offset1));

  // Peek with an offset inside the second write.
  const QuicStreamOffset offset2 = 1500;
  EXPECT_TRUE(buffer_->PeekRegion(offset2, &iov));
  EXPECT_EQ(absl::string_view(source2).substr(offset2 - length1),
            IovecToStringPiece(iov));

  // Peeking at or after FirstMissingByte() returns false.
  EXPECT_FALSE(buffer_->PeekRegion(length1 + length2, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(length1 + length2 + 1, &iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekBufferWithMultipleBlocks) {
  const size_t length1 = 1024;
  std::string source1(length1, 'a');
  buffer_->OnStreamData(0, source1, &written_, &error_details_);

  iovec iov;
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source1, IovecToStringPiece(iov));

  const size_t length2 = kBlockSizeBytes + 2;
  std::string source2(length2, 'b');
  buffer_->OnStreamData(length1, source2, &written_, &error_details_);

  // Peek with offset 0 returns the entire block.
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(kBlockSizeBytes, iov.iov_len);
  EXPECT_EQ(source1, IovecToStringPiece(iov).substr(0, length1));
  EXPECT_EQ(absl::string_view(source2).substr(0, kBlockSizeBytes - length1),
            IovecToStringPiece(iov).substr(length1));

  EXPECT_TRUE(buffer_->PeekRegion(length1, &iov));
  EXPECT_EQ(absl::string_view(source2).substr(0, kBlockSizeBytes - length1),
            IovecToStringPiece(iov));

  EXPECT_TRUE(buffer_->PeekRegion(kBlockSizeBytes, &iov));
  EXPECT_EQ(absl::string_view(source2).substr(kBlockSizeBytes - length1),
            IovecToStringPiece(iov));

  // Peeking at or after FirstMissingByte() returns false.
  EXPECT_FALSE(buffer_->PeekRegion(length1 + length2, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(length1 + length2 + 1, &iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekAfterConsumed) {
  std::string source1(kBlockSizeBytes, 'a');
  buffer_->OnStreamData(0, source1, &written_, &error_details_);

  iovec iov;
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source1, IovecToStringPiece(iov));

  // Consume some data.
  EXPECT_TRUE(buffer_->MarkConsumed(1024));

  // Peeking into consumed data fails.
  EXPECT_FALSE(buffer_->PeekRegion(0, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(512, &iov));

  EXPECT_TRUE(buffer_->PeekRegion(1024, &iov));
  EXPECT_EQ(absl::string_view(source1).substr(1024), IovecToStringPiece(iov));

  EXPECT_TRUE(buffer_->PeekRegion(1500, &iov));
  EXPECT_EQ(absl::string_view(source1).substr(1500), IovecToStringPiece(iov));

  // Consume rest of block.
  EXPECT_TRUE(buffer_->MarkConsumed(kBlockSizeBytes - 1024));

  // Read new data.
  std::string source2(300, 'b');
  buffer_->OnStreamData(kBlockSizeBytes, source2, &written_, &error_details_);

  // Peek into new data.
  EXPECT_TRUE(buffer_->PeekRegion(kBlockSizeBytes, &iov));
  EXPECT_EQ(source2, IovecToStringPiece(iov));

  EXPECT_TRUE(buffer_->PeekRegion(kBlockSizeBytes + 128, &iov));
  EXPECT_EQ(absl::string_view(source2).substr(128), IovecToStringPiece(iov));

  // Peeking into consumed data still fails.
  EXPECT_FALSE(buffer_->PeekRegion(0, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(512, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(1024, &iov));
  EXPECT_FALSE(buffer_->PeekRegion(1500, &iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekContinously) {
  std::string source1(kBlockSizeBytes, 'a');
  buffer_->OnStreamData(0, source1, &written_, &error_details_);

  iovec iov;
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source1, IovecToStringPiece(iov));

  std::string source2(kBlockSizeBytes, 'b');
  buffer_->OnStreamData(kBlockSizeBytes, source2, &written_, &error_details_);

  EXPECT_TRUE(buffer_->PeekRegion(kBlockSizeBytes, &iov));
  EXPECT_EQ(source2, IovecToStringPiece(iov));

  // First block is still there.
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source1, IovecToStringPiece(iov));
}

TEST_F(QuicStreamSequencerBufferTest, PeekRegionWithBufferWrapsAround) {
  ResetMaxCapacityBytes(kBlockSizeBytes * 8);
  std::string source1(kBlockSizeBytes, 'a');
  buffer_->OnStreamData(0, source1, &written_, &error_details_);

  iovec iov;
  EXPECT_TRUE(buffer_->PeekRegion(0, &iov));
  EXPECT_EQ(source1, IovecToStringPiece(iov));
  // Consume half of the block.
  size_t consumed_bytes = kBlockSizeBytes - 4 * 1024;
  buffer_->MarkConsumed(consumed_bytes);

  // Buffer much more data so that the last bytes are stored into the start of
  // the first block, which also contains the first unread bytes.
  std::string source2(max_capacity_bytes_ - 4 * 1024 - 1, 'b');
  buffer_->OnStreamData(kBlockSizeBytes, source2, &written_, &error_details_);
  EXPECT_EQ(max_capacity_bytes_ - 1, buffer_->ReadableBytes());

  EXPECT_TRUE(buffer_->PeekRegion(consumed_bytes, &iov));
  EXPECT_EQ(std::string(4 * 1024, 'a'), IovecToStringPiece(iov));
}

TEST_F(QuicStreamSequencerBufferTest, MarkConsumedInOneBlock) {
  // Write into [0, 1024) and then read out [0, 256)
  std::string source(1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[256];
  helper_->Read(dest, 256);

  EXPECT_TRUE(buffer_->MarkConsumed(512));
  EXPECT_EQ(256u + 512u, buffer_->BytesConsumed());
  EXPECT_EQ(256u, helper_->ReadableBytes());
  buffer_->MarkConsumed(256);
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, MarkConsumedNotEnoughBytes) {
  // Write into [0, 1024) and then read out [0, 256)
  std::string source(1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[256];
  helper_->Read(dest, 256);

  // Consume 1st 512 bytes
  EXPECT_TRUE(buffer_->MarkConsumed(512));
  EXPECT_EQ(256u + 512u, buffer_->BytesConsumed());
  EXPECT_EQ(256u, helper_->ReadableBytes());
  // Try to consume one bytes more than available. Should return false.
  EXPECT_FALSE(buffer_->MarkConsumed(257));
  EXPECT_EQ(256u + 512u, buffer_->BytesConsumed());
  iovec iov;
  EXPECT_TRUE(buffer_->GetReadableRegion(&iov));
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, MarkConsumedAcrossBlock) {
  // Write into [0, 2 * kBlockSizeBytes + 1024) and then read out [0, 1024)
  std::string source(2 * kBlockSizeBytes + 1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[1024];
  helper_->Read(dest, 1024);

  buffer_->MarkConsumed(2 * kBlockSizeBytes);
  EXPECT_EQ(source.size(), buffer_->BytesConsumed());
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, MarkConsumedAcrossEnd) {
  // Write into [0, 8.5 * kBlockSizeBytes - 1024) and then read out [0, 1024)
  // and then append 1024 + 512 bytes.
  std::string source(8.5 * kBlockSizeBytes - 1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[1024];
  helper_->Read(dest, 1024);
  source = std::string(1024 + 512, 'b');
  buffer_->OnStreamData(8.5 * kBlockSizeBytes - 1024, source, &written_,
                        &error_details_);
  EXPECT_EQ(1024u, buffer_->BytesConsumed());

  // Consume to the end of 8th block.
  buffer_->MarkConsumed(8 * kBlockSizeBytes - 1024);
  EXPECT_EQ(8 * kBlockSizeBytes, buffer_->BytesConsumed());
  // Consume across the physical end of buffer
  buffer_->MarkConsumed(0.5 * kBlockSizeBytes + 500);
  EXPECT_EQ(max_capacity_bytes_ + 500, buffer_->BytesConsumed());
  EXPECT_EQ(12u, helper_->ReadableBytes());
  // Consume to the logical end of buffer
  buffer_->MarkConsumed(12);
  EXPECT_EQ(max_capacity_bytes_ + 512, buffer_->BytesConsumed());
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, FlushBufferedFrames) {
  // Write into [0, 8.5 * kBlockSizeBytes - 1024) and then read out [0, 1024).
  std::string source(max_capacity_bytes_ - 1024, 'a');
  buffer_->OnStreamData(0, source, &written_, &error_details_);
  char dest[1024];
  helper_->Read(dest, 1024);
  EXPECT_EQ(1024u, buffer_->BytesConsumed());
  // Write [1024, 512) to the physical beginning.
  source = std::string(512, 'b');
  buffer_->OnStreamData(max_capacity_bytes_, source, &written_,
                        &error_details_);
  EXPECT_EQ(512u, written_);
  EXPECT_EQ(max_capacity_bytes_ - 1024 + 512, buffer_->FlushBufferedFrames());
  EXPECT_EQ(max_capacity_bytes_ + 512, buffer_->BytesConsumed());
  EXPECT_TRUE(buffer_->Empty());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
  // Clear buffer at this point should still preserve BytesConsumed().
  buffer_->Clear();
  EXPECT_EQ(max_capacity_bytes_ + 512, buffer_->BytesConsumed());
  EXPECT_TRUE(helper_->CheckBufferInvariants());
}

TEST_F(QuicStreamSequencerBufferTest, TooManyGaps) {
  // Make sure max capacity is large enough that it is possible to have more
  // than |kMaxNumGapsAllowed| number of gaps.
  max_capacity_bytes_ = 3 * kBlockSizeBytes;
  // Feed buffer with 1-byte discontiguous frames. e.g. [1,2), [3,4), [5,6)...
  for (QuicStreamOffset begin = 1; begin <= max_capacity_bytes_; begin += 2) {
    QuicErrorCode rs =
        buffer_->OnStreamData(begin, "a", &written_, &error_details_);

    QuicStreamOffset last_straw = 2 * kMaxNumGapsAllowed - 1;
    if (begin == last_straw) {
      EXPECT_THAT(rs, IsError(QUIC_TOO_MANY_STREAM_DATA_INTERVALS));
      EXPECT_EQ("Too many data intervals received for this stream.",
                error_details_);
      break;
    }
  }
}

class QuicStreamSequencerBufferRandomIOTest
    : public QuicStreamSequencerBufferTest {
 public:
  using OffsetSizePair = std::pair<QuicStreamOffset, size_t>;

  void SetUp() override {
    // Test against a larger capacity then above tests. Also make sure the last
    // block is partially available to use.
    max_capacity_bytes_ = 8.25 * kBlockSizeBytes;
    // Stream to be buffered should be larger than the capacity to test wrap
    // around.
    bytes_to_buffer_ = 2 * max_capacity_bytes_;
    Initialize();

    uint64_t seed = QuicRandom::GetInstance()->RandUint64();
    QUIC_LOG(INFO) << "**** The current seed is " << seed << " ****";
    rng_.set_seed(seed);
  }

  // Create an out-of-order source stream with given size to populate
  // shuffled_buf_.
  void CreateSourceAndShuffle(size_t max_chunk_size_bytes) {
    max_chunk_size_bytes_ = max_chunk_size_bytes;
    std::unique_ptr<OffsetSizePair[]> chopped_stream(
        new OffsetSizePair[bytes_to_buffer_]);

    // Split stream into small chunks with random length. chopped_stream will be
    // populated with segmented stream chunks.
    size_t start_chopping_offset = 0;
    size_t iterations = 0;
    while (start_chopping_offset < bytes_to_buffer_) {
      size_t max_chunk = std::min<size_t>(
          max_chunk_size_bytes_, bytes_to_buffer_ - start_chopping_offset);
      size_t chunk_size = rng_.RandUint64() % max_chunk + 1;
      chopped_stream[iterations] =
          OffsetSizePair(start_chopping_offset, chunk_size);
      start_chopping_offset += chunk_size;
      ++iterations;
    }
    QUICHE_DCHECK(start_chopping_offset == bytes_to_buffer_);
    size_t chunk_num = iterations;

    // Randomly change the sequence of in-ordered OffsetSizePairs to make a
    // out-of-order array of OffsetSizePairs.
    for (int i = chunk_num - 1; i >= 0; --i) {
      size_t random_idx = rng_.RandUint64() % (i + 1);
      QUIC_DVLOG(1) << "chunk offset " << chopped_stream[random_idx].first
                    << " size " << chopped_stream[random_idx].second;
      shuffled_buf_.push_front(chopped_stream[random_idx]);
      chopped_stream[random_idx] = chopped_stream[i];
    }
  }

  // Write the currently first chunk of data in the out-of-order stream into
  // QuicStreamSequencerBuffer. If current chuck cannot be written into buffer
  // because it goes beyond current capacity, move it to the end of
  // shuffled_buf_ and write it later.
  void WriteNextChunkToBuffer() {
    OffsetSizePair& chunk = shuffled_buf_.front();
    QuicStreamOffset offset = chunk.first;
    const size_t num_to_write = chunk.second;
    std::unique_ptr<char[]> write_buf{new char[max_chunk_size_bytes_]};
    for (size_t i = 0; i < num_to_write; ++i) {
      write_buf[i] = (offset + i) % 256;
    }
    absl::string_view string_piece_w(write_buf.get(), num_to_write);
    auto result = buffer_->OnStreamData(offset, string_piece_w, &written_,
                                        &error_details_);
    if (result == QUIC_NO_ERROR) {
      shuffled_buf_.pop_front();
      total_bytes_written_ += num_to_write;
    } else {
      // This chunk offset exceeds window size.
      shuffled_buf_.push_back(chunk);
      shuffled_buf_.pop_front();
    }
    QUIC_DVLOG(1) << " write at offset: " << offset
                  << " len to write: " << num_to_write
                  << " write result: " << result
                  << " left over: " << shuffled_buf_.size();
  }

 protected:
  std::list<OffsetSizePair> shuffled_buf_;
  size_t max_chunk_size_bytes_;
  QuicStreamOffset bytes_to_buffer_;
  size_t total_bytes_written_ = 0;
  size_t total_bytes_read_ = 0;
  SimpleRandom rng_;
};

TEST_F(QuicStreamSequencerBufferRandomIOTest, RandomWriteAndReadv) {
  // Set kMaxReadSize larger than kBlockSizeBytes to test both small and large
  // read.
  const size_t kMaxReadSize = kBlockSizeBytes * 2;
  // kNumReads is larger than 1 to test how multiple read destinations work.
  const size_t kNumReads = 2;
  // Since write and read operation have equal possibility to be called. Bytes
  // to be written into and read out of should roughly the same.
  const size_t kMaxWriteSize = kNumReads * kMaxReadSize;
  size_t iterations = 0;

  CreateSourceAndShuffle(kMaxWriteSize);

  while ((!shuffled_buf_.empty() || total_bytes_read_ < bytes_to_buffer_) &&
         iterations <= 2 * bytes_to_buffer_) {
    uint8_t next_action =
        shuffled_buf_.empty() ? uint8_t{1} : rng_.RandUint64() % 2;
    QUIC_DVLOG(1) << "iteration: " << iterations;
    switch (next_action) {
      case 0: {  // write
        WriteNextChunkToBuffer();
        ASSERT_TRUE(helper_->CheckBufferInvariants());
        break;
      }
      case 1: {  // readv
        std::unique_ptr<char[][kMaxReadSize]> read_buf{
            new char[kNumReads][kMaxReadSize]};
        iovec dest_iov[kNumReads];
        size_t num_to_read = 0;
        for (size_t i = 0; i < kNumReads; ++i) {
          dest_iov[i].iov_base =
              reinterpret_cast<void*>(const_cast<char*>(read_buf[i]));
          dest_iov[i].iov_len = rng_.RandUint64() % kMaxReadSize;
          num_to_read += dest_iov[i].iov_len;
        }
        size_t actually_read;
        EXPECT_THAT(buffer_->Readv(dest_iov, kNumReads, &actually_read,
                                   &error_details_),
                    IsQuicNoError());
        ASSERT_LE(actually_read, num_to_read);
        QUIC_DVLOG(1) << " read from offset: " << total_bytes_read_
                      << " size: " << num_to_read
                      << " actual read: " << actually_read;
        for (size_t i = 0; i < actually_read; ++i) {
          char ch = (i + total_bytes_read_) % 256;
          ASSERT_EQ(ch, GetCharFromIOVecs(i, dest_iov, kNumReads))
              << " at iteration " << iterations;
        }
        total_bytes_read_ += actually_read;
        ASSERT_EQ(total_bytes_read_, buffer_->BytesConsumed());
        ASSERT_TRUE(helper_->CheckBufferInvariants());
        break;
      }
    }
    ++iterations;
    ASSERT_LE(total_bytes_read_, total_bytes_written_);
  }
  EXPECT_LT(iterations, bytes_to_buffer_) << "runaway test";
  EXPECT_LE(bytes_to_buffer_, total_bytes_read_)
      << "iterations: " << iterations;
  EXPECT_LE(bytes_to_buffer_, total_bytes_written_);
}

TEST_F(QuicStreamSequencerBufferRandomIOTest, RandomWriteAndConsumeInPlace) {
  // The value 4 is chosen such that the max write size is no larger than the
  // maximum buffer capacity.
  const size_t kMaxNumReads = 4;
  // Adjust write amount be roughly equal to that GetReadableRegions() can get.
  const size_t kMaxWriteSize = kMaxNumReads * kBlockSizeBytes;
  ASSERT_LE(kMaxWriteSize, max_capacity_bytes_);
  size_t iterations = 0;

  CreateSourceAndShuffle(kMaxWriteSize);

  while ((!shuffled_buf_.empty() || total_bytes_read_ < bytes_to_buffer_) &&
         iterations <= 2 * bytes_to_buffer_) {
    uint8_t next_action =
        shuffled_buf_.empty() ? uint8_t{1} : rng_.RandUint64() % 2;
    QUIC_DVLOG(1) << "iteration: " << iterations;
    switch (next_action) {
      case 0: {  // write
        WriteNextChunkToBuffer();
        ASSERT_TRUE(helper_->CheckBufferInvariants());
        break;
      }
      case 1: {  // GetReadableRegions and then MarkConsumed
        size_t num_read = rng_.RandUint64() % kMaxNumReads + 1;
        iovec dest_iov[kMaxNumReads];
        ASSERT_TRUE(helper_->CheckBufferInvariants());
        size_t actually_num_read =
            buffer_->GetReadableRegions(dest_iov, num_read);
        ASSERT_LE(actually_num_read, num_read);
        size_t avail_bytes = 0;
        for (size_t i = 0; i < actually_num_read; ++i) {
          avail_bytes += dest_iov[i].iov_len;
        }
        // process random number of bytes (check the value of each byte).
        size_t bytes_to_process = rng_.RandUint64() % (avail_bytes + 1);
        size_t bytes_processed = 0;
        for (size_t i = 0; i < actually_num_read; ++i) {
          size_t bytes_in_block = std::min<size_t>(
              bytes_to_process - bytes_processed, dest_iov[i].iov_len);
          if (bytes_in_block == 0) {
            break;
          }
          for (size_t j = 0; j < bytes_in_block; ++j) {
            ASSERT_LE(bytes_processed, bytes_to_process);
            char char_expected =
                (buffer_->BytesConsumed() + bytes_processed) % 256;
            ASSERT_EQ(char_expected,
                      reinterpret_cast<const char*>(dest_iov[i].iov_base)[j])
                << " at iteration " << iterations;
            ++bytes_processed;
          }
        }

        buffer_->MarkConsumed(bytes_processed);

        QUIC_DVLOG(1) << "iteration " << iterations << ": try to get "
                      << num_read << " readable regions, actually get "
                      << actually_num_read
                      << " from offset: " << total_bytes_read_
                      << "\nprocesse bytes: " << bytes_processed;
        total_bytes_read_ += bytes_processed;
        ASSERT_EQ(total_bytes_read_, buffer_->BytesConsumed());
        ASSERT_TRUE(helper_->CheckBufferInvariants());
        break;
      }
    }
    ++iterations;
    ASSERT_LE(total_bytes_read_, total_bytes_written_);
  }
  EXPECT_LT(iterations, bytes_to_buffer_) << "runaway test";
  EXPECT_LE(bytes_to_buffer_, total_bytes_read_)
      << "iterations: " << iterations;
  EXPECT_LE(bytes_to_buffer_, total_bytes_written_);
}

TEST_F(QuicStreamSequencerBufferTest, GrowBlockSizeOnDemand) {
  max_capacity_bytes_ = 1024 * kBlockSizeBytes;
  std::string source_of_one_block(kBlockSizeBytes, 'a');
  Initialize();

  ASSERT_EQ(helper_->current_blocks_count(), 0u);

  // A minimum of 8 blocks are allocated
  buffer_->OnStreamData(0, source_of_one_block, &written_, &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 8u);

  // Number of blocks doesn't grow if the data is within the capacity.
  buffer_->OnStreamData(kBlockSizeBytes * 7, source_of_one_block, &written_,
                        &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 8u);

  // Number of blocks grows by a factor of 4 normally.
  buffer_->OnStreamData(kBlockSizeBytes * 8, "a", &written_, &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 32u);

  // Number of blocks grow to the demanded size of 140 instead of 128 since
  // that's not enough.
  buffer_->OnStreamData(kBlockSizeBytes * 139, source_of_one_block, &written_,
                        &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 140u);

  // Number of blocks grows by a factor of 4 normally.
  buffer_->OnStreamData(kBlockSizeBytes * 140, source_of_one_block, &written_,
                        &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 560u);

  // max_capacity_bytes is reached and number of blocks is capped.
  buffer_->OnStreamData(kBlockSizeBytes * 560, source_of_one_block, &written_,
                        &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 1024u);

  // max_capacity_bytes is reached and number of blocks is capped.
  buffer_->OnStreamData(kBlockSizeBytes * 1025, source_of_one_block, &written_,
                        &error_details_);
  ASSERT_EQ(helper_->current_blocks_count(), 1024u);
}

}  // anonymous namespace

}  // namespace test

}  // namespace quic
