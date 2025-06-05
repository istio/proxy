// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/core/spdy_frame_builder.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "absl/strings/string_view.h"
#include "quiche/http2/core/array_output_buffer.h"
#include "quiche/http2/core/spdy_protocol.h"
#include "quiche/http2/test_tools/spdy_test_utils.h"
#include "quiche/common/platform/api/quiche_export.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace spdy {

namespace test {

class QUICHE_EXPORT SpdyFrameBuilderPeer {
 public:
  static char* GetWritableBuffer(SpdyFrameBuilder* builder, size_t length) {
    return builder->GetWritableBuffer(length);
  }

  static char* GetWritableOutput(SpdyFrameBuilder* builder,
                                 size_t desired_length, size_t* actual_length) {
    return builder->GetWritableOutput(desired_length, actual_length);
  }
};

namespace {

const int64_t kSize = 64 * 1024;
char output_buffer[kSize] = "";

}  // namespace

// Verifies that SpdyFrameBuilder::GetWritableBuffer() can be used to build a
// SpdySerializedFrame.
TEST(SpdyFrameBuilderTest, GetWritableBuffer) {
  const size_t kBuilderSize = 10;
  SpdyFrameBuilder builder(kBuilderSize);
  char* writable_buffer =
      SpdyFrameBuilderPeer::GetWritableBuffer(&builder, kBuilderSize);
  memset(writable_buffer, ~1, kBuilderSize);
  EXPECT_TRUE(builder.Seek(kBuilderSize));
  SpdySerializedFrame frame(builder.take());
  char expected[kBuilderSize];
  memset(expected, ~1, kBuilderSize);
  EXPECT_EQ(absl::string_view(expected, kBuilderSize), frame);
}

// Verifies that SpdyFrameBuilder::GetWritableBuffer() can be used to build a
// SpdySerializedFrame to the output buffer.
TEST(SpdyFrameBuilderTest, GetWritableOutput) {
  ArrayOutputBuffer output(output_buffer, kSize);
  const size_t kBuilderSize = 10;
  SpdyFrameBuilder builder(kBuilderSize, &output);
  size_t actual_size = 0;
  char* writable_buffer = SpdyFrameBuilderPeer::GetWritableOutput(
      &builder, kBuilderSize, &actual_size);
  memset(writable_buffer, ~1, kBuilderSize);
  EXPECT_TRUE(builder.Seek(kBuilderSize));
  SpdySerializedFrame frame = MakeSerializedFrame(output.Begin(), kBuilderSize);
  char expected[kBuilderSize];
  memset(expected, ~1, kBuilderSize);
  EXPECT_EQ(absl::string_view(expected, kBuilderSize), frame);
}

// Verifies the case that the buffer's capacity is too small.
TEST(SpdyFrameBuilderTest, GetWritableOutputNegative) {
  size_t small_cap = 1;
  ArrayOutputBuffer output(output_buffer, small_cap);
  const size_t kBuilderSize = 10;
  SpdyFrameBuilder builder(kBuilderSize, &output);
  size_t actual_size = 0;
  char* writable_buffer = SpdyFrameBuilderPeer::GetWritableOutput(
      &builder, kBuilderSize, &actual_size);
  EXPECT_EQ(0u, actual_size);
  EXPECT_EQ(nullptr, writable_buffer);
}

}  // namespace test
}  // namespace spdy
