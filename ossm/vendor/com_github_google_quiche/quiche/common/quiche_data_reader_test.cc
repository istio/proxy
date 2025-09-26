// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_data_reader.h"

#include <cstdint>

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_endian.h"

namespace quiche {

// TODO(b/214573190): Test Endianness::HOST_BYTE_ORDER.
// TODO(b/214573190): Test ReadUInt8, ReadUInt24, ReadUInt64, ReadBytesToUInt64,
// ReadStringPiece8, ReadStringPiece, ReadTag, etc.

TEST(QuicheDataReaderTest, ReadUInt16) {
  // Data in network byte order.
  const uint16_t kData[] = {
      QuicheEndian::HostToNet16(1),
      QuicheEndian::HostToNet16(1 << 15),
  };

  QuicheDataReader reader(reinterpret_cast<const char*>(kData), sizeof(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  uint16_t uint16_val;
  EXPECT_TRUE(reader.ReadUInt16(&uint16_val));
  EXPECT_FALSE(reader.IsDoneReading());
  EXPECT_EQ(1, uint16_val);

  EXPECT_TRUE(reader.ReadUInt16(&uint16_val));
  EXPECT_TRUE(reader.IsDoneReading());
  EXPECT_EQ(1 << 15, uint16_val);
}

TEST(QuicheDataReaderTest, ReadUInt32) {
  // Data in network byte order.
  const uint32_t kData[] = {
      QuicheEndian::HostToNet32(1),
      QuicheEndian::HostToNet32(0x80000000),
  };

  QuicheDataReader reader(reinterpret_cast<const char*>(kData),
                          ABSL_ARRAYSIZE(kData) * sizeof(uint32_t));
  EXPECT_FALSE(reader.IsDoneReading());

  uint32_t uint32_val;
  EXPECT_TRUE(reader.ReadUInt32(&uint32_val));
  EXPECT_FALSE(reader.IsDoneReading());
  EXPECT_EQ(1u, uint32_val);

  EXPECT_TRUE(reader.ReadUInt32(&uint32_val));
  EXPECT_TRUE(reader.IsDoneReading());
  EXPECT_EQ(1u << 31, uint32_val);
}

TEST(QuicheDataReaderTest, ReadStringPiece16) {
  // Data in network byte order.
  const char kData[] = {
      0x00, 0x02,  // uint16_t(2)
      0x48, 0x69,  // "Hi"
      0x00, 0x10,  // uint16_t(16)
      0x54, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67, 0x2c,
      0x20, 0x31, 0x2c, 0x20, 0x32, 0x2c, 0x20, 0x33,  // "Testing, 1, 2, 3"
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  absl::string_view stringpiece_val;
  EXPECT_TRUE(reader.ReadStringPiece16(&stringpiece_val));
  EXPECT_FALSE(reader.IsDoneReading());
  EXPECT_EQ(0, stringpiece_val.compare("Hi"));

  EXPECT_TRUE(reader.ReadStringPiece16(&stringpiece_val));
  EXPECT_TRUE(reader.IsDoneReading());
  EXPECT_EQ(0, stringpiece_val.compare("Testing, 1, 2, 3"));
}

TEST(QuicheDataReaderTest, ReadUInt16WithBufferTooSmall) {
  // Data in network byte order.
  const char kData[] = {
      0x00,  // part of a uint16_t
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  uint16_t uint16_val;
  EXPECT_FALSE(reader.ReadUInt16(&uint16_val));
}

TEST(QuicheDataReaderTest, ReadUInt32WithBufferTooSmall) {
  // Data in network byte order.
  const char kData[] = {
      0x00, 0x00, 0x00,  // part of a uint32_t
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  uint32_t uint32_val;
  EXPECT_FALSE(reader.ReadUInt32(&uint32_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(reader.ReadUInt16(&uint16_val));
}

// Tests ReadStringPiece16() with a buffer too small to fit the entire string.
TEST(QuicheDataReaderTest, ReadStringPiece16WithBufferTooSmall) {
  // Data in network byte order.
  const char kData[] = {
      0x00, 0x03,  // uint16_t(3)
      0x48, 0x69,  // "Hi"
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  absl::string_view stringpiece_val;
  EXPECT_FALSE(reader.ReadStringPiece16(&stringpiece_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(reader.ReadUInt16(&uint16_val));
}

// Tests ReadStringPiece16() with a buffer too small even to fit the length.
TEST(QuicheDataReaderTest, ReadStringPiece16WithBufferWayTooSmall) {
  // Data in network byte order.
  const char kData[] = {
      0x00,  // part of a uint16_t
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  absl::string_view stringpiece_val;
  EXPECT_FALSE(reader.ReadStringPiece16(&stringpiece_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(reader.ReadUInt16(&uint16_val));
}

TEST(QuicheDataReaderTest, ReadBytes) {
  // Data in network byte order.
  const char kData[] = {
      0x66, 0x6f, 0x6f,  // "foo"
      0x48, 0x69,        // "Hi"
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  char dest1[3] = {};
  EXPECT_TRUE(reader.ReadBytes(&dest1, ABSL_ARRAYSIZE(dest1)));
  EXPECT_FALSE(reader.IsDoneReading());
  EXPECT_EQ("foo", absl::string_view(dest1, ABSL_ARRAYSIZE(dest1)));

  char dest2[2] = {};
  EXPECT_TRUE(reader.ReadBytes(&dest2, ABSL_ARRAYSIZE(dest2)));
  EXPECT_TRUE(reader.IsDoneReading());
  EXPECT_EQ("Hi", absl::string_view(dest2, ABSL_ARRAYSIZE(dest2)));
}

TEST(QuicheDataReaderTest, ReadBytesWithBufferTooSmall) {
  // Data in network byte order.
  const char kData[] = {
      0x01,
  };

  QuicheDataReader reader(kData, ABSL_ARRAYSIZE(kData));
  EXPECT_FALSE(reader.IsDoneReading());

  char dest[ABSL_ARRAYSIZE(kData) + 2] = {};
  EXPECT_FALSE(reader.ReadBytes(&dest, ABSL_ARRAYSIZE(kData) + 1));
  EXPECT_STREQ("", dest);
}

TEST(QuicheDataReaderTest, ReadAtMost) {
  constexpr absl::string_view kData = "foobar";
  QuicheDataReader reader(kData);
  EXPECT_EQ(reader.ReadAtMost(0), "");
  EXPECT_EQ(reader.ReadAtMost(3), "foo");
  EXPECT_EQ(reader.ReadAtMost(6), "bar");
  EXPECT_EQ(reader.ReadAtMost(1000), "");
}

}  // namespace quiche
