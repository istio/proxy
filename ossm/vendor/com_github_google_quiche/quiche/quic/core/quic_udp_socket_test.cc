// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_udp_socket.h"

#include <netinet/in.h>
#include <stdint.h>

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "quiche/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

const int kReceiveBufferSize = 16000;
const int kSendBufferSize = 16000;

class QuicUdpSocketTest : public QuicTest {
 protected:
  ABSL_CACHELINE_ALIGNED char packet_buffer_[20];
  ABSL_CACHELINE_ALIGNED char control_buffer_[512];
};

TEST_F(QuicUdpSocketTest, Basic) {
  const QuicSocketAddress any_address(quiche::QuicheIpAddress::Any6(), 0);
  QuicUdpSocketApi socket_api;

  SocketFd server_socket =
      socket_api.Create(AF_INET6, kSendBufferSize, kReceiveBufferSize);
  ASSERT_NE(kQuicInvalidSocketFd, server_socket);
  ASSERT_TRUE(socket_api.Bind(server_socket, any_address));
  QuicSocketAddress server_address;
  ASSERT_EQ(0, server_address.FromSocket(server_socket));

  SocketFd client_socket =
      socket_api.Create(AF_INET6, kSendBufferSize, kReceiveBufferSize);
  ASSERT_NE(kQuicInvalidSocketFd, client_socket);
  ASSERT_TRUE(socket_api.Bind(client_socket, any_address));
  QuicSocketAddress client_address;
  ASSERT_EQ(0, client_address.FromSocket(client_socket));

  QuicUdpPacketInfo packet_info;
  packet_info.SetPeerAddress(server_address);

  WriteResult write_result;
  const absl::string_view client_data = "acd";
  write_result = socket_api.WritePacket(client_socket, client_data.data(),
                                        client_data.length(), packet_info);
  ASSERT_EQ(WRITE_STATUS_OK, write_result.status);

  QuicUdpPacketInfoBitMask packet_info_interested;
  QuicUdpSocketApi::ReadPacketResult read_result;
  read_result.packet_buffer = {&packet_buffer_[0], sizeof(packet_buffer_)};
  read_result.control_buffer = {&control_buffer_[0], sizeof(control_buffer_)};

  socket_api.ReadPacket(server_socket, packet_info_interested, &read_result);
  ASSERT_TRUE(read_result.ok);
  ASSERT_EQ(client_data,
            absl::string_view(read_result.packet_buffer.buffer,
                              read_result.packet_buffer.buffer_len));

  const absl::string_view server_data = "acd";
  packet_info.SetPeerAddress(client_address);
  write_result = socket_api.WritePacket(server_socket, server_data.data(),
                                        server_data.length(), packet_info);
  ASSERT_EQ(WRITE_STATUS_OK, write_result.status);

  read_result.Reset(sizeof(packet_buffer_));
  socket_api.ReadPacket(client_socket, packet_info_interested, &read_result);
  ASSERT_TRUE(read_result.ok);
  ASSERT_EQ(server_data,
            absl::string_view(read_result.packet_buffer.buffer,
                              read_result.packet_buffer.buffer_len));
}

TEST_F(QuicUdpSocketTest, FlowLabel) {
  const QuicSocketAddress any_address(quiche::QuicheIpAddress::Any6(), 0);
  QuicUdpSocketApi socket_api;

  SocketFd server_socket =
      socket_api.Create(AF_INET6, kSendBufferSize, kReceiveBufferSize);
  ASSERT_NE(kQuicInvalidSocketFd, server_socket);
  ASSERT_TRUE(socket_api.Bind(server_socket, any_address));
  QuicSocketAddress server_address;
  ASSERT_EQ(0, server_address.FromSocket(server_socket));

  SocketFd client_socket =
      socket_api.Create(AF_INET6, kSendBufferSize, kReceiveBufferSize);
  ASSERT_NE(kQuicInvalidSocketFd, client_socket);
  ASSERT_TRUE(socket_api.Bind(client_socket, any_address));
  QuicSocketAddress client_address;
  ASSERT_EQ(0, client_address.FromSocket(client_socket));

  const absl::string_view data = "a";
  const uint32_t client_flow_label = 1;
  QuicUdpPacketInfo packet_info;
  packet_info.SetFlowLabel(client_flow_label);
  packet_info.SetPeerAddress(server_address);

  WriteResult write_result;
  write_result = socket_api.WritePacket(client_socket, data.data(),
                                        data.length(), packet_info);
  ASSERT_EQ(WRITE_STATUS_OK, write_result.status);

  QuicUdpPacketInfoBitMask packet_info_interested(
      {quic::QuicUdpPacketInfoBit::V6_FLOW_LABEL});
  QuicUdpSocketApi::ReadPacketResult read_result;
  read_result.packet_buffer = {&packet_buffer_[0], sizeof(packet_buffer_)};
  read_result.control_buffer = {&control_buffer_[0], sizeof(control_buffer_)};

  do {
    socket_api.ReadPacket(server_socket, packet_info_interested, &read_result);
  } while (!read_result.ok);
#if !defined(__ANDROID__)
  ASSERT_TRUE(
      read_result.packet_info.HasValue(QuicUdpPacketInfoBit::V6_FLOW_LABEL));
  EXPECT_EQ(client_flow_label, read_result.packet_info.flow_label());
#else
  EXPECT_FALSE(
      read_result.packet_info.HasValue(QuicUdpPacketInfoBit::V6_FLOW_LABEL));
#endif

  const uint32_t server_flow_label = 3;
  packet_info.SetPeerAddress(client_address);
  packet_info.SetFlowLabel(server_flow_label);
  write_result = socket_api.WritePacket(server_socket, data.data(),
                                        data.length(), packet_info);
  ASSERT_EQ(WRITE_STATUS_OK, write_result.status);

  read_result.Reset(sizeof(packet_buffer_));
  do {
    socket_api.ReadPacket(client_socket, packet_info_interested, &read_result);
  } while (!read_result.ok);
#if !defined(__ANDROID__)
  ASSERT_TRUE(
      read_result.packet_info.HasValue(QuicUdpPacketInfoBit::V6_FLOW_LABEL));
  EXPECT_EQ(server_flow_label, read_result.packet_info.flow_label());
#else
  EXPECT_FALSE(
      read_result.packet_info.HasValue(QuicUdpPacketInfoBit::V6_FLOW_LABEL));
#endif
}

}  // namespace
}  // namespace test
}  // namespace quic
