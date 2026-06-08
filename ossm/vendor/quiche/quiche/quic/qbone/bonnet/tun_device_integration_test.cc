// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/if_tun.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/io/socket.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_ip_address_family.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_thread.h"
#include "quiche/quic/qbone/bonnet/tun_device.h"
#include "quiche/quic/qbone/bonnet/tun_device_controller.h"
#include "quiche/quic/qbone/platform/ip_range.h"
#include "quiche/quic/qbone/platform/kernel_interface.h"
#include "quiche/quic/qbone/platform/netlink.h"
#include "quiche/quic/test_tools/test_ip_packets.h"
#include "quiche/common/internet_checksum.h"
#include "quiche/common/platform/api/quiche_logging.h"

namespace quic::test {
namespace {

// Tests for TunDevice that rely on the real kernel and bring up a real tun
// device. Mostly functions as an experimentation playground for poking at TUN.
class TunDeviceIntegrationTest : public QuicTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(local_address_.FromString("2001:db8:2026:1::"));
    ASSERT_TRUE(remote_address_.FromString("2001:db8:2026:2::"));

    std::string interface_name = absl::StrFormat(
        "qbone-test-%d",
        QuicRandom::GetInstance()->InsecureRandUint64() % 10000);
    tun_device_ = std::make_unique<TunTapDevice>(
        interface_name, /*mtu=*/1600, /*persist=*/false, /*setup_tun=*/true,
        /*is_tap=*/false, &kernel_);
    tun_device_controller_ = std::make_unique<TunDeviceController>(
        interface_name, /*setup_tun=*/true, &netlink_);
  }

  QuicIpAddress local_address_;
  QuicIpAddress remote_address_;

  Kernel kernel_;
  Netlink netlink_{&kernel_};
  std::unique_ptr<TunTapDevice> tun_device_;
  std::unique_ptr<TunDeviceController> tun_device_controller_;
};

// Probably not necessary for TUN devices since TunTapDevice already opens the
// device in non-blocking mode, but good to make sure.
absl::Status SetNonBlocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return absl::ErrnoToStatus(errno, "Failed to get flags");
  }
  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to set flags");
  }
  return absl::OkStatus();
}

TEST_F(TunDeviceIntegrationTest, MassiveNumWrites) {
  ASSERT_TRUE(tun_device_->Init());
  ASSERT_GT(tun_device_->GetWriteFileDescriptor(), -1);
  ASSERT_TRUE(tun_device_controller_->UpdateAddress(
      IpRange(local_address_, /*prefix_length=*/64)));
  ASSERT_TRUE(tun_device_->Up());

  int sndbuf = 500;
  ASSERT_GE(kernel_.ioctl(tun_device_->GetWriteFileDescriptor(), TUNSETSNDBUF,
                          &sndbuf),
            0);

  ASSERT_OK(SetNonBlocking(tun_device_->GetWriteFileDescriptor()));

  QuicSocketAddress source_endpoint(remote_address_, /*port=*/53368);
  QuicSocketAddress destination_endpoint(local_address_, /*port=*/56362);
  std::string payload(256, 'a');
  std::string packet = CreateIpPacket(
      source_endpoint.host(), destination_endpoint.host(),
      CreateUdpPacket(source_endpoint, destination_endpoint, payload));

  absl::StatusOr<SocketFd> udp_socket = socket_api::CreateSocket(
      IpAddressFamily::IP_V6, socket_api::SocketProtocol::kUdp,
      /*blocking=*/false);
  ASSERT_OK(udp_socket);
  OwnedSocketFd owned_udp_socket(udp_socket.value());

  ASSERT_OK(socket_api::Bind(udp_socket.value(), destination_endpoint));

  std::vector<char> receive_buffer(1600);
  for (int i = 0; i < 1000000; ++i) {
    ASSERT_EQ(kernel_.write(tun_device_->GetWriteFileDescriptor(),
                            packet.data(), packet.size()),
              packet.size())
        << "Write failed on iteration " << i << " with error " << errno;

    absl::StatusOr<absl::Span<char>> receive_data =
        socket_api::Receive(udp_socket.value(), absl::MakeSpan(receive_buffer));
    ASSERT_OK(receive_data)
        << "Receive failed on iteration " << i << " with error "
        << receive_data.status().message();
  }
}

TEST_F(TunDeviceIntegrationTest, MassiveWrite) {
  ASSERT_TRUE(tun_device_->Init());
  ASSERT_GT(tun_device_->GetWriteFileDescriptor(), -1);
  ASSERT_TRUE(tun_device_controller_->UpdateAddress(
      IpRange(local_address_, /*prefix_length=*/64)));
  ASSERT_TRUE(tun_device_->Up());

  QuicSocketAddress source_endpoint(remote_address_, /*port=*/53368);
  QuicSocketAddress destination_endpoint(local_address_, /*port=*/56362);
  std::string payload(65527, 'a');
  std::string packet = CreateIpPacket(
      source_endpoint.host(), destination_endpoint.host(),
      CreateUdpPacket(source_endpoint, destination_endpoint, payload));

  absl::StatusOr<SocketFd> udp_socket = socket_api::CreateSocket(
      IpAddressFamily::IP_V6, socket_api::SocketProtocol::kUdp,
      /*blocking=*/false);
  ASSERT_OK(udp_socket);
  OwnedSocketFd owned_udp_socket(udp_socket.value());

  ASSERT_OK(socket_api::Bind(udp_socket.value(), destination_endpoint));

  ASSERT_EQ(kernel_.write(tun_device_->GetWriteFileDescriptor(), packet.data(),
                          packet.size()),
            packet.size());

  std::vector<char> receive_buffer(payload.size() + 1000);
  absl::StatusOr<absl::Span<char>> receive_data =
      socket_api::Receive(udp_socket.value(), absl::MakeSpan(receive_buffer));
  ASSERT_OK(receive_data);
  ASSERT_EQ(receive_data->size(), payload.size());
}

// Useful so a connected TCP socket can disappear immediately after the test is
// done rather than hanging around to wait for graceful TCP termination.
absl::Status DisableLinger(SocketFd fd) {
  struct linger linger;
  linger.l_onoff = 1;
  linger.l_linger = 0;
  if (::setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to set SO_LINGER");
  }
  return absl::OkStatus();
}

// Skips the IPv6 header and known extension headers (like hop-by-hop options).
absl::Span<const uint8_t> SkipHeader(absl::Span<const uint8_t> packet,
                                     uint8_t* out_next_header) {
  QUICHE_CHECK_GE(packet.size(), sizeof(ip6_hdr));
  const ip6_hdr* ip_header = reinterpret_cast<const ip6_hdr*>(packet.data());
  QUICHE_CHECK_EQ(ip_header->ip6_vfc >> 4, 6);
  *out_next_header = ip_header->ip6_nxt;
  packet = packet.subspan(sizeof(ip6_hdr));
  while (*out_next_header == IPPROTO_HOPOPTS) {
    QUICHE_CHECK_GE(packet.size(), sizeof(ip6_hbh));
    const ip6_hbh* hbh_header = reinterpret_cast<const ip6_hbh*>(packet.data());
    QUICHE_CHECK_GE(packet.size(), 8 + (8 * hbh_header->ip6h_len));
    *out_next_header = hbh_header->ip6h_nxt;
    packet = packet.subspan(8 + (8 * hbh_header->ip6h_len));
  }
  return packet;
}

// Returns true if packet is an ignorable ICMPv6 packet that we simply don't
// care about.
bool IsGarbagePacket(absl::Span<const uint8_t> packet) {
  uint8_t next_header;
  absl::Span<const uint8_t> inner_packet = SkipHeader(packet, &next_header);
  if (next_header != IPPROTO_ICMPV6) {
    return false;
  }

  QUICHE_CHECK_GE(inner_packet.size(), sizeof(icmp6_hdr));
  const icmp6_hdr* icmp6_header =
      reinterpret_cast<const icmp6_hdr*>(inner_packet.data());
  switch (icmp6_header->icmp6_type) {
    case ND_ROUTER_SOLICIT:
    case 143:  // Multicast Listener Discovery (MLDv2 Listener Report)
      return true;
    default:
      QUICHE_CHECK(false)
          << "Unexpected ICMPv6 type: " << icmp6_header->icmp6_type
          << ". Should it be added to the garbage packet filter list?";
      return false;
  }
}

void SkipGarbagePackets(int file_descriptor, Kernel* kernel) {
  std::vector<uint8_t> read_buffer(3200);
  ssize_t bytes_read = -1;
  do {
    bytes_read =
        kernel->read(file_descriptor, read_buffer.data(), read_buffer.size());
  } while (bytes_read > 0 &&
           IsGarbagePacket(absl::MakeSpan(read_buffer.data(), bytes_read)));
  ASSERT_LT(bytes_read, 0);
}

std::vector<uint8_t> ReadTcpPacket(int file_descriptor, Kernel* kernel,
                                   absl::Duration timeout,
                                   const QuicIpAddress& expected_source,
                                   const QuicIpAddress& expected_destination) {
  std::vector<uint8_t> read_buffer(3200);
  ssize_t bytes_read = -1;
  absl::Time deadline = absl::Now() + timeout;
  while (true) {
    bytes_read =
        kernel->read(file_descriptor, read_buffer.data(), read_buffer.size());
    if (bytes_read > 0 &&
        IsGarbagePacket(absl::MakeSpan(read_buffer.data(), bytes_read))) {
      continue;
    }

    if (bytes_read > 0) {
      break;
    }
    QUICHE_CHECK_EQ(errno, EWOULDBLOCK);

    if (absl::Now() > deadline) {
      return {};
    }

    absl::SleepFor(absl::Milliseconds(1));
  }

  QUICHE_CHECK_GT(bytes_read, 0)
      << "Failed to read packet: " << strerror(errno);
  QUICHE_CHECK_GE(bytes_read, sizeof(ip6_hdr));
  const ip6_hdr* ip_header =
      reinterpret_cast<const ip6_hdr*>(read_buffer.data());
  QUICHE_CHECK_EQ(ip_header->ip6_vfc >> 4, 6);
  QUICHE_CHECK_EQ(QuicIpAddress(ip_header->ip6_src), expected_source);
  QUICHE_CHECK_EQ(QuicIpAddress(ip_header->ip6_dst), expected_destination);

  uint8_t next_header;
  absl::Span<const uint8_t> tcp_packet =
      SkipHeader(absl::MakeSpan(read_buffer.data(), bytes_read), &next_header);
  QUICHE_CHECK_EQ(next_header, IPPROTO_TCP);
  QUICHE_CHECK_GE(tcp_packet.size(), sizeof(tcphdr));

  return std::vector<uint8_t>(tcp_packet.begin(), tcp_packet.end());
}

void UpdateTcpChecksum(tcphdr* tcp_header, const QuicIpAddress& source_address,
                       const QuicIpAddress& destination_address,
                       absl::Span<const uint8_t> payload) {
  quiche::InternetChecksum checksum;
  checksum.Update(source_address.ToPackedString());
  checksum.Update(destination_address.ToPackedString());
  uint8_t protocol[] = {0x00, IPPROTO_TCP};
  checksum.Update(protocol, sizeof(protocol));
  uint16_t tcp_length = htons(sizeof(tcphdr) + payload.size());
  checksum.Update(reinterpret_cast<uint8_t*>(&tcp_length), sizeof(tcp_length));
  checksum.Update(reinterpret_cast<const uint8_t*>(tcp_header), sizeof(tcphdr));
  checksum.Update(payload.data(), payload.size());

  tcp_header->check = checksum.Value();
}

class TcpReceiveThread : public QuicThread {
 public:
  TcpReceiveThread(OwnedSocketFd tcp_socket, absl::Notification* stop)
      : QuicThread("TcpReceiveThread"),
        tcp_socket_(std::move(tcp_socket)),
        stop_(stop) {}

  void Run() override {
    receive_buffer_.resize(3200);

    for (;;) {
      if (stop_->HasBeenNotified()) {
        break;
      }
      absl::StatusOr<absl::Span<char>> receive_data = socket_api::Receive(
          tcp_socket_.get(), absl::MakeSpan(receive_buffer_));

      if (receive_data.ok()) {
        bytes_received_ += receive_data.value().size();
      } else {
        QUICHE_CHECK_EQ(receive_data.status().code(),
                        absl::StatusCode::kUnavailable);
        absl::SleepFor(absl::Milliseconds(1));
      }
    }
  }

  int bytes_received() const { return bytes_received_; }

 private:
  OwnedSocketFd tcp_socket_;
  std::vector<char> receive_buffer_;
  int bytes_received_ = 0;
  absl::Notification* stop_;
};

TEST_F(TunDeviceIntegrationTest, TcpConnection) {
  ASSERT_TRUE(tun_device_->Init());
  ASSERT_GT(tun_device_->GetWriteFileDescriptor(), -1);
  ASSERT_TRUE(tun_device_controller_->UpdateAddress(
      IpRange(local_address_, /*prefix_length=*/64)));
  ASSERT_TRUE(tun_device_->Up());
  ASSERT_TRUE(tun_device_controller_->UpdateRoutes(
      IpRange(local_address_, /*prefix_length=*/64),
      {IpRange(remote_address_, /*prefix_length=*/64)}));

  ASSERT_OK(SetNonBlocking(tun_device_->GetReadFileDescriptor()));
  SkipGarbagePackets(tun_device_->GetReadFileDescriptor(), &kernel_);

  QuicSocketAddress client_endpoint(remote_address_, /*port=*/55171);
  QuicSocketAddress server_endpoint(local_address_, /*port=*/60722);

  absl::StatusOr<SocketFd> tcp_socket = socket_api::CreateSocket(
      IpAddressFamily::IP_V6, socket_api::SocketProtocol::kTcp,
      /*blocking=*/false);
  ASSERT_OK(tcp_socket);
  OwnedSocketFd owned_tcp_socket(tcp_socket.value());

  ASSERT_OK(socket_api::Bind(tcp_socket.value(), server_endpoint));
  ASSERT_OK(socket_api::Listen(tcp_socket.value(), 5));

  tcphdr tcp_header;
  ::memset(&tcp_header, 0, sizeof(tcp_header));
  tcp_header.source = htons(client_endpoint.port());
  tcp_header.dest = htons(server_endpoint.port());
  tcp_header.seq = htonl(142);
  tcp_header.doff = 5;
  tcp_header.syn = 1;
  UpdateTcpChecksum(&tcp_header, client_endpoint.host(), server_endpoint.host(),
                    /*payload=*/{});
  std::string syn_packet = CreateIpPacket(
      client_endpoint.host(), server_endpoint.host(),
      absl::string_view(reinterpret_cast<const char*>(&tcp_header),
                        sizeof(tcp_header)),
      IpPacketPayloadType::kTcp);

  ASSERT_EQ(kernel_.write(tun_device_->GetWriteFileDescriptor(),
                          syn_packet.data(), syn_packet.size()),
            syn_packet.size());

  std::vector<uint8_t> syn_ack_packet = ReadTcpPacket(
      tun_device_->GetReadFileDescriptor(), &kernel_, absl::Seconds(10),
      server_endpoint.host(), client_endpoint.host());
  ASSERT_GE(syn_ack_packet.size(), sizeof(tcphdr));
  const tcphdr* syn_ack_tcp_header =
      reinterpret_cast<const tcphdr*>(syn_ack_packet.data());
  ASSERT_EQ(syn_ack_tcp_header->syn, 1);
  ASSERT_EQ(syn_ack_tcp_header->ack, 1);
  ASSERT_EQ(ntohl(syn_ack_tcp_header->ack_seq), 143);

  ::memset(&tcp_header, 0, sizeof(tcp_header));
  tcp_header.source = htons(client_endpoint.port());
  tcp_header.dest = htons(server_endpoint.port());
  tcp_header.seq = htonl(143);
  tcp_header.ack_seq = htonl(ntohl(syn_ack_tcp_header->seq) + 1);
  tcp_header.doff = 5;
  tcp_header.ack = 1;
  UpdateTcpChecksum(&tcp_header, client_endpoint.host(), server_endpoint.host(),
                    /*payload=*/{});
  std::string ack_packet = CreateIpPacket(
      client_endpoint.host(), server_endpoint.host(),
      absl::string_view(reinterpret_cast<const char*>(&tcp_header),
                        sizeof(tcp_header)),
      IpPacketPayloadType::kTcp);

  ASSERT_EQ(kernel_.write(tun_device_->GetWriteFileDescriptor(),
                          ack_packet.data(), ack_packet.size()),
            ack_packet.size());

  absl::StatusOr<socket_api::AcceptResult> accept_result =
      socket_api::Accept(tcp_socket.value(), /*blocking=*/false);
  ASSERT_OK(accept_result);
  OwnedSocketFd connected_socket(accept_result->fd);
  ASSERT_OK(DisableLinger(connected_socket.get()));
  ASSERT_EQ(accept_result->peer_address, client_endpoint);

  absl::Notification stop_receive_thread;
  TcpReceiveThread tcp_receive_thread(std::move(connected_socket),
                                      &stop_receive_thread);
  tcp_receive_thread.Start();

  ::memset(&tcp_header, 0, sizeof(tcp_header));
  tcp_header.source = htons(client_endpoint.port());
  tcp_header.dest = htons(server_endpoint.port());
  tcp_header.ack_seq = htonl(ntohl(syn_ack_tcp_header->seq) + 1);
  tcp_header.doff = 5;
  tcp_header.ack = 1;
  std::string payload(100, 'a');
  int sequence_number = 143;
  int highest_ack_seq = 0;
  for (int i = 0; i < 1000000; ++i) {
    tcp_header.seq = htonl(sequence_number);
    tcp_header.check = 0;
    UpdateTcpChecksum(
        &tcp_header, client_endpoint.host(), server_endpoint.host(),
        absl::MakeSpan(reinterpret_cast<const uint8_t*>(payload.data()),
                       payload.size()));
    std::string combined_payload = absl::StrCat(
        absl::string_view(reinterpret_cast<const char*>(&tcp_header),
                          sizeof(tcphdr)),
        payload);
    std::string packet =
        CreateIpPacket(client_endpoint.host(), server_endpoint.host(),
                       combined_payload, IpPacketPayloadType::kTcp);
    ASSERT_EQ(kernel_.write(tun_device_->GetWriteFileDescriptor(),
                            packet.data(), packet.size()),
              packet.size());

    sequence_number += payload.size();

    bool zero_window = false;
    for (;;) {
      std::vector<uint8_t> response_packet = ReadTcpPacket(
          tun_device_->GetReadFileDescriptor(), &kernel_, absl::ZeroDuration(),
          server_endpoint.host(), client_endpoint.host());
      if (response_packet.empty()) {
        break;
      }
      ASSERT_GE(response_packet.size(), sizeof(tcphdr));
      const tcphdr* response_tcp_header =
          reinterpret_cast<const tcphdr*>(response_packet.data());
      ASSERT_EQ(response_tcp_header->ack, 1);
      ASSERT_GE(ntohl(response_tcp_header->ack_seq), highest_ack_seq);
      ASSERT_EQ(ntohl(response_tcp_header->seq),
                ntohl(syn_ack_tcp_header->seq) + 1);

      if (ntohs(response_tcp_header->window) == 0) {
        zero_window = true;
        QUICHE_LOG(INFO)
            << "Window is zero, stopping massive writes at iteration " << i;
        break;
      }

      if (ntohl(response_tcp_header->ack_seq) > highest_ack_seq) {
        highest_ack_seq = ntohl(response_tcp_header->ack_seq);
        if (highest_ack_seq > sequence_number) {
          // Missed packets have been filled in, so we can jump back ahead.
          sequence_number = highest_ack_seq;
        }
      } else {
        // Revert and retry at missed sequence number.
        sequence_number = ntohl(response_tcp_header->ack_seq);
      }
    }

    if (zero_window) {
      break;
    }
  }

  for (;;) {
    std::vector<uint8_t> response_packet = ReadTcpPacket(
        tun_device_->GetReadFileDescriptor(), &kernel_, absl::Seconds(5),
        server_endpoint.host(), client_endpoint.host());
    if (response_packet.empty()) {
      break;
    }
    ASSERT_GE(response_packet.size(), sizeof(tcphdr));
    const tcphdr* response_tcp_header =
        reinterpret_cast<const tcphdr*>(response_packet.data());
    ASSERT_EQ(response_tcp_header->ack, 1);
    ASSERT_GE(ntohl(response_tcp_header->ack_seq), highest_ack_seq);
    ASSERT_EQ(ntohl(response_tcp_header->seq),
              ntohl(syn_ack_tcp_header->seq) + 1);
    highest_ack_seq = ntohl(response_tcp_header->ack_seq);
  }

  stop_receive_thread.Notify();
  tcp_receive_thread.Join();
}

}  // namespace
}  // namespace quic::test
