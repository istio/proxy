// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/qbone/qbone_packet_exchanger.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/qbone/mock_qbone_client.h"

namespace quic {
namespace {

using ::testing::StrEq;
using ::testing::StrictMock;

class MockVisitor : public QbonePacketExchanger::Visitor {
 public:
  MOCK_METHOD(void, OnReadError, (const std::string&), (override));
  MOCK_METHOD(void, OnWriteError, (const std::string&), (override));
  MOCK_METHOD(absl::Status, OnWrite, (absl::string_view), (override));
};

class FakeQbonePacketExchanger : public QbonePacketExchanger {
 public:
  using QbonePacketExchanger::QbonePacketExchanger;

  // Adds a packet to the end of list of packets to be returned by ReadPacket.
  // When the list is empty, ReadPacket returns nullptr to signify error as
  // defined by QbonePacketExchanger. If SetReadError is not called or called
  // with empty error string, ReadPacket sets blocked to true.
  void AddPacketToBeRead(std::unique_ptr<QuicData> packet) {
    packets_to_be_read_.push_back(std::move(packet));
  }

  // Sets the error to be returned by ReadPacket when the list of packets is
  // empty. If error is empty string, blocked is set by ReadPacket.
  void SetReadError(const std::string& error) { read_error_ = error; }

  // Force WritePacket to fail with the given status.
  void ForceWriteFailure(const std::string& error) { write_error_ = error; }

  // Packets that have been successfully written by WritePacket.
  const std::vector<std::string>& packets_written() const {
    return packets_written_;
  }

 private:
  // Implements QbonePacketExchanger::ReadPacket.
  std::unique_ptr<QuicData> ReadPacket(std::string* error) override {
    if (packets_to_be_read_.empty()) {
      *error = read_error_;
      return nullptr;
    }

    std::unique_ptr<QuicData> packet = std::move(packets_to_be_read_.front());
    packets_to_be_read_.pop_front();
    return packet;
  }

  // Implements QbonePacketExchanger::WritePacket.
  bool WritePacket(const char* packet, size_t size,
                   std::string* error) override {
    if (!write_error_.empty()) {
      *error = write_error_;
      return false;
    }

    packets_written_.push_back(std::string(packet, size));
    return true;
  }

  std::string read_error_;
  std::list<std::unique_ptr<QuicData>> packets_to_be_read_;

  std::string write_error_;
  std::vector<std::string> packets_written_;
};

TEST(QbonePacketExchangerTest,
     ReadAndDeliverPacketDeliversPacketToQboneClient) {
  StrictMock<MockVisitor> visitor;
  FakeQbonePacketExchanger exchanger(&visitor);
  StrictMock<MockQboneClient> client;

  std::string packet = "data";
  exchanger.AddPacketToBeRead(
      std::make_unique<QuicData>(packet.data(), packet.length()));
  EXPECT_CALL(client, ProcessPacketFromNetwork(StrEq("data")));

  EXPECT_TRUE(exchanger.ReadAndDeliverPacket(&client));
}

TEST(QbonePacketExchangerTest,
     ReadAndDeliverPacketNotifiesVisitorOnReadFailure) {
  MockVisitor visitor;
  FakeQbonePacketExchanger exchanger(&visitor);
  MockQboneClient client;

  // Force read error.
  std::string io_error = "I/O error";
  exchanger.SetReadError(io_error);
  EXPECT_CALL(visitor, OnReadError(StrEq(io_error))).Times(1);

  EXPECT_FALSE(exchanger.ReadAndDeliverPacket(&client));
}

TEST(QbonePacketExchangerTest,
     ReadAndDeliverPacketDoesNotNotifyVisitorOnBlockedIO) {
  MockVisitor visitor;
  FakeQbonePacketExchanger exchanger(&visitor);
  MockQboneClient client;

  // No more packets to read.
  EXPECT_FALSE(exchanger.ReadAndDeliverPacket(&client));
}

TEST(QbonePacketExchangerTest,
     WritePacketToNetworkWritesDirectlyToNetworkWhenNotBlocked) {
  MockVisitor visitor;
  FakeQbonePacketExchanger exchanger(&visitor);
  MockQboneClient client;

  std::string packet = "data";
  exchanger.WritePacketToNetwork(packet.data(), packet.length());

  ASSERT_EQ(exchanger.packets_written().size(), 1);
  EXPECT_THAT(exchanger.packets_written()[0], StrEq(packet));
}

TEST(QbonePacketExchangerTest, WritePacketToNetworkDropsPacketIfBlocked) {
  std::vector<std::string> packets = {"packet0", "packet1", "packet2"};
  MockVisitor visitor;
  FakeQbonePacketExchanger exchanger(&visitor);
  MockQboneClient client;

  exchanger.ForceWriteFailure("blocked");
  for (int i = 0; i < packets.size(); i++) {
    exchanger.WritePacketToNetwork(packets[i].data(), packets[i].length());
  }

  // Blocked writes cause packets to be dropped.
  ASSERT_TRUE(exchanger.packets_written().empty());
}

TEST(QbonePacketExchangerTest, WriteErrorsGetNotified) {
  MockVisitor visitor;
  FakeQbonePacketExchanger exchanger(&visitor);
  MockQboneClient client;
  std::string packet = "data";

  // Write error is delivered to visitor during WritePacketToNetwork.
  std::string io_error = "I/O error";
  exchanger.ForceWriteFailure(io_error);
  EXPECT_CALL(visitor, OnWriteError(StrEq(io_error))).Times(1);
  exchanger.WritePacketToNetwork(packet.data(), packet.length());
  ASSERT_TRUE(exchanger.packets_written().empty());
}

TEST(QbonePacketExchangerTest, NullVisitorDoesntCrash) {
  FakeQbonePacketExchanger exchanger(nullptr);
  MockQboneClient client;
  std::string packet = "data";

  // Force read error.
  std::string io_error = "I/O error";
  exchanger.SetReadError(io_error);
  EXPECT_FALSE(exchanger.ReadAndDeliverPacket(&client));

  // Force write error
  exchanger.ForceWriteFailure(io_error);
  exchanger.WritePacketToNetwork(packet.data(), packet.length());
  EXPECT_TRUE(exchanger.packets_written().empty());
}

}  // namespace
}  // namespace quic
