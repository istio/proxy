// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/balsa/simple_buffer.h"

#include <string>

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {

namespace test {

namespace {

constexpr int kMinimumSimpleBufferSize = 10;

// Buffer full of 40 char strings.
const char ibuf[] = {
    "123456789!@#$%^&*()abcdefghijklmnopqrstu"
    "123456789!@#$%^&*()abcdefghijklmnopqrstu"
    "123456789!@#$%^&*()abcdefghijklmnopqrstu"
    "123456789!@#$%^&*()abcdefghijklmnopqrstu"
    "123456789!@#$%^&*()abcdefghijklmnopqrstu"};

}  // namespace

class SimpleBufferTest : public QuicheTest {
 public:
  static char* storage(SimpleBuffer& buffer) { return buffer.storage_; }
  static int write_idx(SimpleBuffer& buffer) { return buffer.write_idx_; }
  static int read_idx(SimpleBuffer& buffer) { return buffer.read_idx_; }
  static int storage_size(SimpleBuffer& buffer) { return buffer.storage_size_; }
};

namespace {

TEST_F(SimpleBufferTest, CreationWithSize) {
  SimpleBuffer buffer1(5);
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer1));

  SimpleBuffer buffer2(25);
  EXPECT_EQ(25, storage_size(buffer2));
}

// Make sure that a zero-sized initial buffer does not throw things off.
TEST_F(SimpleBufferTest, CreationWithZeroSize) {
  SimpleBuffer buffer(0);
  EXPECT_EQ(0, storage_size(buffer));
  EXPECT_EQ(4, buffer.Write(ibuf, 4));
  EXPECT_EQ(4, write_idx(buffer));
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));
  EXPECT_EQ(4, buffer.ReadableBytes());
}

TEST_F(SimpleBufferTest, ReadZeroBytes) {
  SimpleBuffer buffer;

  EXPECT_EQ(0, buffer.Read(nullptr, 0));
}

TEST_F(SimpleBufferTest, WriteZeroFromNullptr) {
  SimpleBuffer buffer;

  EXPECT_EQ(0, buffer.Write(nullptr, 0));
}

TEST(SimpleBufferExpectBug, ReserveNegativeSize) {
  SimpleBuffer buffer;

  EXPECT_QUICHE_BUG(buffer.Reserve(-1), "size must not be negative");
}

TEST(SimpleBufferExpectBug, ReadNegativeSize) {
  SimpleBuffer buffer;

  EXPECT_QUICHE_BUG(buffer.Read(nullptr, -1), "size must not be negative");
}

TEST(SimpleBufferExpectBug, WriteNegativeSize) {
  SimpleBuffer buffer;

  EXPECT_QUICHE_BUG(buffer.Write(nullptr, -1), "size must not be negative");
}

TEST_F(SimpleBufferTest, Basics) {
  SimpleBuffer buffer;

  EXPECT_TRUE(buffer.Empty());
  EXPECT_EQ("", buffer.GetReadableRegion());
  EXPECT_EQ(0, storage_size(buffer));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));

  char* readable_ptr = nullptr;
  int readable_size = 0;
  buffer.GetReadablePtr(&readable_ptr, &readable_size);
  char* writeable_ptr = nullptr;
  int writable_size = 0;
  buffer.GetWritablePtr(&writeable_ptr, &writable_size);

  EXPECT_EQ(storage(buffer), readable_ptr);
  EXPECT_EQ(0, readable_size);
  EXPECT_EQ(storage(buffer), writeable_ptr);
  EXPECT_EQ(0, writable_size);
  EXPECT_EQ(0, buffer.ReadableBytes());

  const SimpleBuffer buffer2;
  EXPECT_EQ(0, buffer2.ReadableBytes());
}

TEST_F(SimpleBufferTest, BasicWR) {
  SimpleBuffer buffer;

  EXPECT_EQ(4, buffer.Write(ibuf, 4));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(4, write_idx(buffer));
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));
  EXPECT_EQ(4, buffer.ReadableBytes());
  EXPECT_EQ("1234", buffer.GetReadableRegion());
  int bytes_written = 4;
  EXPECT_TRUE(!buffer.Empty());

  char* readable_ptr = nullptr;
  int readable_size = 0;
  buffer.GetReadablePtr(&readable_ptr, &readable_size);
  char* writeable_ptr = nullptr;
  int writable_size = 0;
  buffer.GetWritablePtr(&writeable_ptr, &writable_size);

  EXPECT_EQ(storage(buffer), readable_ptr);
  EXPECT_EQ(4, readable_size);
  EXPECT_EQ(storage(buffer) + 4, writeable_ptr);
  EXPECT_EQ(6, writable_size);

  char obuf[ABSL_ARRAYSIZE(ibuf)];
  int bytes_read = 0;
  EXPECT_EQ(4, buffer.Read(obuf + bytes_read, 40));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));
  EXPECT_EQ(0, buffer.ReadableBytes());
  EXPECT_EQ("", buffer.GetReadableRegion());
  bytes_read += 4;
  EXPECT_TRUE(buffer.Empty());
  buffer.GetReadablePtr(&readable_ptr, &readable_size);
  buffer.GetWritablePtr(&writeable_ptr, &writable_size);
  EXPECT_EQ(storage(buffer), readable_ptr);
  EXPECT_EQ(0, readable_size);
  EXPECT_EQ(storage(buffer), writeable_ptr);
  EXPECT_EQ(kMinimumSimpleBufferSize, writable_size);

  EXPECT_EQ(bytes_written, bytes_read);
  for (int i = 0; i < bytes_read; ++i) {
    EXPECT_EQ(obuf[i], ibuf[i]);
  }

  // More R/W tests.
  EXPECT_EQ(10, buffer.Write(ibuf + bytes_written, 10));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(10, write_idx(buffer));
  EXPECT_EQ(10, storage_size(buffer));
  EXPECT_EQ(10, buffer.ReadableBytes());
  bytes_written += 10;

  EXPECT_TRUE(!buffer.Empty());

  EXPECT_EQ(6, buffer.Read(obuf + bytes_read, 6));
  EXPECT_EQ(6, read_idx(buffer));
  EXPECT_EQ(10, write_idx(buffer));
  EXPECT_EQ(10, storage_size(buffer));
  EXPECT_EQ(4, buffer.ReadableBytes());
  bytes_read += 6;

  EXPECT_TRUE(!buffer.Empty());

  EXPECT_EQ(4, buffer.Read(obuf + bytes_read, 7));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));
  EXPECT_EQ(10, storage_size(buffer));
  EXPECT_EQ(0, buffer.ReadableBytes());
  bytes_read += 4;

  EXPECT_TRUE(buffer.Empty());

  EXPECT_EQ(bytes_written, bytes_read);
  for (int i = 0; i < bytes_read; ++i) {
    EXPECT_EQ(obuf[i], ibuf[i]);
  }
}

TEST_F(SimpleBufferTest, Reserve) {
  SimpleBuffer buffer;
  EXPECT_EQ(0, storage_size(buffer));

  buffer.WriteString("foo");
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));

  // Reserve by expanding the buffer.
  buffer.Reserve(kMinimumSimpleBufferSize + 1);
  EXPECT_EQ(2 * kMinimumSimpleBufferSize, storage_size(buffer));

  buffer.Clear();
  buffer.AdvanceWritablePtr(kMinimumSimpleBufferSize);
  buffer.AdvanceReadablePtr(kMinimumSimpleBufferSize - 2);
  EXPECT_EQ(kMinimumSimpleBufferSize, write_idx(buffer));
  EXPECT_EQ(2 * kMinimumSimpleBufferSize, storage_size(buffer));

  // Reserve by moving data around.  `storage_size` does not change.
  buffer.Reserve(kMinimumSimpleBufferSize + 1);
  EXPECT_EQ(2, write_idx(buffer));
  EXPECT_EQ(2 * kMinimumSimpleBufferSize, storage_size(buffer));
}

TEST_F(SimpleBufferTest, Extend) {
  SimpleBuffer buffer;

  // Test a write which should not extend the buffer.
  EXPECT_EQ(7, buffer.Write(ibuf, 7));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(7, write_idx(buffer));
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));
  EXPECT_EQ(7, buffer.ReadableBytes());
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(7, write_idx(buffer));
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));
  EXPECT_EQ(7, buffer.ReadableBytes());
  int bytes_written = 7;

  // Test a write which should extend the buffer.
  EXPECT_EQ(4, buffer.Write(ibuf + bytes_written, 4));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(11, write_idx(buffer));
  EXPECT_EQ(20, storage_size(buffer));
  EXPECT_EQ(11, buffer.ReadableBytes());
  bytes_written += 4;

  char obuf[ABSL_ARRAYSIZE(ibuf)];
  EXPECT_EQ(11, buffer.Read(obuf, 11));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));
  EXPECT_EQ(20, storage_size(buffer));
  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));
  EXPECT_EQ(0, buffer.ReadableBytes());

  const int bytes_read = 11;
  EXPECT_EQ(bytes_written, bytes_read);
  for (int i = 0; i < bytes_read; ++i) {
    EXPECT_EQ(obuf[i], ibuf[i]);
  }
}

TEST_F(SimpleBufferTest, Clear) {
  SimpleBuffer buffer;

  buffer.Clear();

  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));
  EXPECT_EQ(0, storage_size(buffer));
  EXPECT_EQ(0, buffer.ReadableBytes());

  buffer.WriteString("foo");
  buffer.Clear();

  EXPECT_EQ(0, read_idx(buffer));
  EXPECT_EQ(0, write_idx(buffer));
  EXPECT_EQ(kMinimumSimpleBufferSize, storage_size(buffer));
  EXPECT_EQ(0, buffer.ReadableBytes());
}

TEST_F(SimpleBufferTest, LongWrite) {
  SimpleBuffer buffer;

  std::string s1 = "HTTP/1.1 500 Service Unavailable";
  buffer.Write(s1.data(), s1.size());
  buffer.Write("\r\n", 2);
  std::string key = "Connection";
  std::string value = "close";
  buffer.Write(key.data(), key.size());
  buffer.Write(": ", 2);
  buffer.Write(value.data(), value.size());
  buffer.Write("\r\n", 2);
  buffer.Write("\r\n", 2);
  std::string message =
      "<html><head>\n"
      "<meta http-equiv=\"content-type\""
      " content=\"text/html;charset=us-ascii\">\n"
      "<style><!--\n"
      "body {font-family: arial,sans-serif}\n"
      "div.nav {margin-top: 1ex}\n"
      "div.nav A {font-size: 10pt; font-family: arial,sans-serif}\n"
      "span.nav {font-size: 10pt; font-family: arial,sans-serif;"
      " font-weight: bold}\n"
      "div.nav A,span.big {font-size: 12pt; color: #0000cc}\n"
      "div.nav A {font-size: 10pt; color: black}\n"
      "A.l:link {color: #6f6f6f}\n"
      "A.u:link {color: green}\n"
      "//--></style>\n"
      "</head>\n"
      "<body text=#000000 bgcolor=#ffffff>\n"
      "<table border=0 cellpadding=2 cellspacing=0 width=100%>"
      "<tr><td rowspan=3 width=1% nowrap>\n"
      "<b>"
      "<font face=times color=#0039b6 size=10>G</font>"
      "<font face=times color=#c41200 size=10>o</font>"
      "<font face=times color=#f3c518 size=10>o</font>"
      "<font face=times color=#0039b6 size=10>g</font>"
      "<font face=times color=#30a72f size=10>l</font>"
      "<font face=times color=#c41200 size=10>e</font>"
      "&nbsp;&nbsp;</b>\n"
      "<td>&nbsp;</td></tr>\n"
      "<tr><td bgcolor=#3366cc><font face=arial,sans-serif color=#ffffff>"
      " <b>Error</b></td></tr>\n"
      "<tr><td>&nbsp;</td></tr></table>\n"
      "<blockquote>\n"
      "<H1> Internal Server Error</H1>\n"
      " This server was unable to complete the request\n"
      "<p></blockquote>\n"
      "<table width=100% cellpadding=0 cellspacing=0>"
      "<tr><td bgcolor=#3366cc><img alt=\"\" width=1 height=4></td></tr>"
      "</table>"
      "</body></html>\n";
  buffer.Write(message.data(), message.size());
  const std::string correct_result =
      "HTTP/1.1 500 Service Unavailable\r\n"
      "Connection: close\r\n"
      "\r\n"
      "<html><head>\n"
      "<meta http-equiv=\"content-type\""
      " content=\"text/html;charset=us-ascii\">\n"
      "<style><!--\n"
      "body {font-family: arial,sans-serif}\n"
      "div.nav {margin-top: 1ex}\n"
      "div.nav A {font-size: 10pt; font-family: arial,sans-serif}\n"
      "span.nav {font-size: 10pt; font-family: arial,sans-serif;"
      " font-weight: bold}\n"
      "div.nav A,span.big {font-size: 12pt; color: #0000cc}\n"
      "div.nav A {font-size: 10pt; color: black}\n"
      "A.l:link {color: #6f6f6f}\n"
      "A.u:link {color: green}\n"
      "//--></style>\n"
      "</head>\n"
      "<body text=#000000 bgcolor=#ffffff>\n"
      "<table border=0 cellpadding=2 cellspacing=0 width=100%>"
      "<tr><td rowspan=3 width=1% nowrap>\n"
      "<b>"
      "<font face=times color=#0039b6 size=10>G</font>"
      "<font face=times color=#c41200 size=10>o</font>"
      "<font face=times color=#f3c518 size=10>o</font>"
      "<font face=times color=#0039b6 size=10>g</font>"
      "<font face=times color=#30a72f size=10>l</font>"
      "<font face=times color=#c41200 size=10>e</font>"
      "&nbsp;&nbsp;</b>\n"
      "<td>&nbsp;</td></tr>\n"
      "<tr><td bgcolor=#3366cc><font face=arial,sans-serif color=#ffffff>"
      " <b>Error</b></td></tr>\n"
      "<tr><td>&nbsp;</td></tr></table>\n"
      "<blockquote>\n"
      "<H1> Internal Server Error</H1>\n"
      " This server was unable to complete the request\n"
      "<p></blockquote>\n"
      "<table width=100% cellpadding=0 cellspacing=0>"
      "<tr><td bgcolor=#3366cc><img alt=\"\" width=1 height=4></td></tr>"
      "</table>"
      "</body></html>\n";
  EXPECT_EQ(correct_result, buffer.GetReadableRegion());
}

TEST_F(SimpleBufferTest, ReleaseAsSlice) {
  SimpleBuffer buffer;

  buffer.WriteString("abc");
  SimpleBuffer::ReleasedBuffer released = buffer.Release();
  EXPECT_EQ("abc", absl::string_view(released.buffer.get(), released.size));

  char* readable_ptr = nullptr;
  int readable_size = 0;
  buffer.GetReadablePtr(&readable_ptr, &readable_size);
  EXPECT_EQ(nullptr, readable_ptr);
  EXPECT_EQ(0, readable_size);

  buffer.WriteString("def");
  released = buffer.Release();
  buffer.GetReadablePtr(&readable_ptr, &readable_size);
  EXPECT_EQ(nullptr, readable_ptr);
  EXPECT_EQ(0, readable_size);
  EXPECT_EQ("def", absl::string_view(released.buffer.get(), released.size));
}

TEST_F(SimpleBufferTest, EmptyBufferReleaseAsSlice) {
  SimpleBuffer buffer;
  char* readable_ptr = nullptr;
  int readable_size = 0;

  SimpleBuffer::ReleasedBuffer released = buffer.Release();
  buffer.GetReadablePtr(&readable_ptr, &readable_size);
  EXPECT_EQ(nullptr, readable_ptr);
  EXPECT_EQ(0, readable_size);
  EXPECT_TRUE(released.buffer == nullptr);
  EXPECT_EQ(released.size, 0u);
}

}  // namespace

}  // namespace test

}  // namespace quiche
