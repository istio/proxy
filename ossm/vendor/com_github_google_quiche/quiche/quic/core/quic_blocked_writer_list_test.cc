// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_blocked_writer_list.h"

#include "quiche/quic/core/quic_blocked_writer_interface.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {

using testing::Invoke;
using testing::Return;

namespace {
class TestWriter : public QuicBlockedWriterInterface {
 public:
  ~TestWriter() override = default;

  MOCK_METHOD(void, OnBlockedWriterCanWrite, ());
  MOCK_METHOD(bool, IsWriterBlocked, (), (const));
};
}  // namespace

TEST(QuicBlockedWriterList, Empty) {
  QuicBlockedWriterList list;
  EXPECT_TRUE(list.Empty());
}

TEST(QuicBlockedWriterList, NotEmpty) {
  QuicBlockedWriterList list;
  testing::StrictMock<TestWriter> writer1;
  EXPECT_CALL(writer1, IsWriterBlocked()).WillOnce(Return(true));
  list.Add(writer1);
  EXPECT_FALSE(list.Empty());
  list.Remove(writer1);
  EXPECT_TRUE(list.Empty());
}

TEST(QuicBlockedWriterList, OnWriterUnblocked) {
  QuicBlockedWriterList list;
  testing::StrictMock<TestWriter> writer1;

  EXPECT_CALL(writer1, IsWriterBlocked()).WillOnce(Return(true));
  list.Add(writer1);
  EXPECT_CALL(writer1, OnBlockedWriterCanWrite());
  list.OnWriterUnblocked();
  EXPECT_TRUE(list.Empty());
}

TEST(QuicBlockedWriterList, OnWriterUnblockedInOrder) {
  QuicBlockedWriterList list;
  testing::StrictMock<TestWriter> writer1;
  testing::StrictMock<TestWriter> writer2;
  testing::StrictMock<TestWriter> writer3;

  EXPECT_CALL(writer1, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer2, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer3, IsWriterBlocked()).WillOnce(Return(true));

  list.Add(writer1);
  list.Add(writer2);
  list.Add(writer3);

  testing::InSequence s;
  EXPECT_CALL(writer1, OnBlockedWriterCanWrite());
  EXPECT_CALL(writer2, OnBlockedWriterCanWrite());
  EXPECT_CALL(writer3, OnBlockedWriterCanWrite());
  list.OnWriterUnblocked();
  EXPECT_TRUE(list.Empty());
}

TEST(QuicBlockedWriterList, OnWriterUnblockedInOrderAfterReinsertion) {
  QuicBlockedWriterList list;
  testing::StrictMock<TestWriter> writer1;
  testing::StrictMock<TestWriter> writer2;
  testing::StrictMock<TestWriter> writer3;

  EXPECT_CALL(writer1, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer2, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer3, IsWriterBlocked()).WillOnce(Return(true));

  list.Add(writer1);
  list.Add(writer2);
  list.Add(writer3);

  EXPECT_CALL(writer1, IsWriterBlocked()).WillOnce(Return(true));
  list.Add(writer1);

  testing::InSequence s;
  EXPECT_CALL(writer1, OnBlockedWriterCanWrite());
  EXPECT_CALL(writer2, OnBlockedWriterCanWrite());
  EXPECT_CALL(writer3, OnBlockedWriterCanWrite());
  list.OnWriterUnblocked();
  EXPECT_TRUE(list.Empty());
}

TEST(QuicBlockedWriterList, OnWriterUnblockedThenBlocked) {
  QuicBlockedWriterList list;
  testing::StrictMock<TestWriter> writer1;
  testing::StrictMock<TestWriter> writer2;
  testing::StrictMock<TestWriter> writer3;

  EXPECT_CALL(writer1, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer2, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer3, IsWriterBlocked()).WillOnce(Return(true));

  list.Add(writer1);
  list.Add(writer2);
  list.Add(writer3);

  EXPECT_CALL(writer1, OnBlockedWriterCanWrite());
  EXPECT_CALL(writer2, IsWriterBlocked()).WillOnce(Return(true));
  EXPECT_CALL(writer2, OnBlockedWriterCanWrite()).WillOnce(Invoke([&]() {
    list.Add(writer2);
  }));

  EXPECT_CALL(writer3, OnBlockedWriterCanWrite());
  list.OnWriterUnblocked();
  EXPECT_FALSE(list.Empty());

  EXPECT_CALL(writer2, OnBlockedWriterCanWrite());
  list.OnWriterUnblocked();
  EXPECT_TRUE(list.Empty());
}

}  // namespace quic
