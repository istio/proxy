// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
// This must be defined before including any system headers.
#define __APPLE_USE_RFC_3542
#endif  // defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)

#include "quiche/quic/core/quic_udp_socket.h"

#include <algorithm>
#include <string>

#include "absl/base/optimization.h"
#include "quiche/quic/core/io/socket.h"
#include "quiche/quic/platform/api/quic_bug_tracker.h"
#include "quiche/quic/platform/api/quic_flag_utils.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/common/quiche_callbacks.h"

// Common cmsg-related functions are defined below.
// Windows and POSIX cmsg formats are actually fairly similar, except the
// Windows ones have all of the macros prefixed with WSA_ and all the type names
// are different.

namespace quic {
namespace {

#if defined(_WIN32)
using PlatformCmsghdr = ::WSACMSGHDR;
#if !defined(CMSG_DATA)
#define CMSG_DATA WSA_CMSG_DATA
#endif  // !defined(CMSG_DATA)
#else
using PlatformCmsghdr = ::cmsghdr;
#endif  // defined(_WIN32)

void PopulatePacketInfoFromControlMessageBase(
    PlatformCmsghdr* cmsg, QuicUdpPacketInfo* packet_info,
    QuicUdpPacketInfoBitMask packet_info_interested) {
  if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::V6_SELF_IP)) {
      const in6_pktinfo* info = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
      const char* addr_data = reinterpret_cast<const char*>(&info->ipi6_addr);
      int addr_len = sizeof(in6_addr);
      QuicIpAddress self_v6_ip;
      if (self_v6_ip.FromPackedString(addr_data, addr_len)) {
        packet_info->SetSelfV6Ip(self_v6_ip);
      } else {
        QUIC_BUG(quic_bug_10751_1) << "QuicIpAddress::FromPackedString failed";
      }
    }
    return;
  }

  if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::V4_SELF_IP)) {
      const in_pktinfo* info = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
      const char* addr_data = reinterpret_cast<const char*>(&info->ipi_addr);
      int addr_len = sizeof(in_addr);
      QuicIpAddress self_v4_ip;
      if (self_v4_ip.FromPackedString(addr_data, addr_len)) {
        packet_info->SetSelfV4Ip(self_v4_ip);
      } else {
        QUIC_BUG(quic_bug_10751_2) << "QuicIpAddress::FromPackedString failed";
      }
    }
    return;
  }
}

}  // namespace
}  // namespace quic

#if defined(_WIN32)
#include "quiche/quic/core/quic_udp_socket_win.inc"
#else
#include "quiche/quic/core/quic_udp_socket_posix.inc"
#endif

namespace quic {

QuicUdpSocketFd QuicUdpSocketApi::Create(int address_family,
                                         int receive_buffer_size,
                                         int send_buffer_size, bool ipv6_only) {
  // QUICHE_DCHECK here so the program exits early(before reading packets) in
  // debug mode. This should have been a static_assert, however it can't be done
  // on ios/osx because CMSG_SPACE isn't a constant expression there.
  QUICHE_DCHECK_GE(kDefaultUdpPacketControlBufferSize, kMinCmsgSpaceForRead);

  absl::StatusOr<SocketFd> socket = socket_api::CreateSocket(
      quiche::FromPlatformAddressFamily(address_family),
      socket_api::SocketProtocol::kUdp,
      /*blocking=*/false);

  if (!socket.ok()) {
    QUIC_LOG_FIRST_N(ERROR, 100)
        << "UDP non-blocking socket creation for address_family="
        << address_family << " failed: " << socket.status();
    return kQuicInvalidSocketFd;
  }

#if !defined(_WIN32)
  SetGoogleSocketOptions(*socket);
#endif

  if (!SetupSocket(*socket, address_family, receive_buffer_size,
                   send_buffer_size, ipv6_only)) {
    Destroy(*socket);
    return kQuicInvalidSocketFd;
  }

  return *socket;
}

void QuicUdpSocketApi::Destroy(QuicUdpSocketFd fd) {
  if (fd != kQuicInvalidSocketFd) {
    absl::Status result = socket_api::Close(fd);
    if (!result.ok()) {
      QUIC_LOG_FIRST_N(WARNING, 100)
          << "Failed to close UDP socket with error " << result;
    }
  }
}

bool QuicUdpSocketApi::Bind(QuicUdpSocketFd fd, QuicSocketAddress address) {
  sockaddr_storage addr = address.generic_address();
  int addr_len =
      address.host().IsIPv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
  return 0 == bind(fd, reinterpret_cast<sockaddr*>(&addr), addr_len);
}

bool QuicUdpSocketApi::BindInterface(QuicUdpSocketFd fd,
                                     const std::string& interface_name) {
#if defined(__linux__) && !defined(__ANDROID_API__)
  if (interface_name.empty() || interface_name.size() >= IFNAMSIZ) {
    QUIC_BUG(udp_bad_interface_name)
        << "interface_name must be nonempty and shorter than " << IFNAMSIZ;
    return false;
  }

  return 0 == setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
                         interface_name.c_str(), interface_name.length());
#else
  (void)fd;
  (void)interface_name;
  QUIC_BUG(interface_bind_not_implemented)
      << "Interface binding is not implemented on this platform";
  return false;
#endif
}

bool QuicUdpSocketApi::EnableDroppedPacketCount(QuicUdpSocketFd fd) {
#if defined(__linux__) && defined(SO_RXQ_OVFL)
  int get_overflow = 1;
  return 0 == setsockopt(fd, SOL_SOCKET, SO_RXQ_OVFL, &get_overflow,
                         sizeof(get_overflow));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableReceiveSelfIpAddressForV4(QuicUdpSocketFd fd) {
  int get_self_ip = 1;
  return 0 == setsockopt(fd, IPPROTO_IP, IP_PKTINFO,
                         reinterpret_cast<char*>(&get_self_ip),
                         sizeof(get_self_ip));
}

bool QuicUdpSocketApi::EnableReceiveSelfIpAddressForV6(QuicUdpSocketFd fd) {
  int get_self_ip = 1;
  return 0 == setsockopt(fd, IPPROTO_IPV6, kIpv6RecvPacketInfo,
                         reinterpret_cast<char*>(&get_self_ip),
                         sizeof(get_self_ip));
}

bool QuicUdpSocketApi::EnableReceiveTimestamp(QuicUdpSocketFd fd) {
#if defined(QUIC_UDP_SOCKET_SUPPORT_LINUX_TIMESTAMPING)
  int timestamping = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
  return 0 == setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamping,
                         sizeof(timestamping));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableReceiveTtlForV4(QuicUdpSocketFd fd) {
#if defined(QUIC_UDP_SOCKET_SUPPORT_TTL)
  int get_ttl = 1;
  return 0 == setsockopt(fd, IPPROTO_IP, IP_RECVTTL, &get_ttl, sizeof(get_ttl));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableReceiveTtlForV6(QuicUdpSocketFd fd) {
#if defined(QUIC_UDP_SOCKET_SUPPORT_TTL)
  int get_ttl = 1;
  return 0 == setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &get_ttl,
                         sizeof(get_ttl));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableUdpGro(QuicUdpSocketFd fd) {
#if defined(__linux__) && defined(SOL_UDP)
  int enable_gro = 1;
  return 0 == setsockopt(fd, SOL_UDP, UDP_GRO, &enable_gro, sizeof(enable_gro));
#else
  (void)fd;
  return false;
#endif
}

size_t QuicUdpSocketApi::ForEachGroSegment(
    const ReadPacketResult& result,
    quiche::UnretainedCallback<void(const ReadPacketResult&)> packet_sink) {
  if (!result.ok) {
    return 0;
  }
  if (!result.packet_info.HasValue(QuicUdpPacketInfoBit::IS_GRO)) {
    QUIC_DVLOG(3) << "Dispatching non-coalesced packet with size: "
                  << result.packet_buffer.buffer_len;
    packet_sink(result);
    return 1;
  }

  const size_t gso_total_size = result.packet_buffer.buffer_len;
  const size_t gso_size = result.packet_info.gso_size();
  QUIC_DVLOG(2) << "Read GRO train with total size: " << gso_total_size
                << " and gso size: " << gso_size;
  if (gso_size == 0) {
    QUIC_BUG(quic_udp_socket_zero_gso_size) << "Invalid gso size of zero.";
    return 0;
  }

  // Slice the coalesced UDP GRO train into individual segments. All segments
  // are `gso_size` bytes, except the trailing segment which may be smaller.
  ReadPacketResult gro_slice = result;
  size_t num_segments = 0;
  size_t bytes_processed = 0;
  while (bytes_processed < gso_total_size) {
    gro_slice.packet_buffer.buffer =
        result.packet_buffer.buffer + bytes_processed;
    gro_slice.packet_buffer.buffer_len =
        std::min(gso_size, gso_total_size - bytes_processed);
    QUIC_DVLOG(3) << "Dispatching GRO segment #" << num_segments
                  << " with size: " << gro_slice.packet_buffer.buffer_len;
    packet_sink(gro_slice);
    bytes_processed += gro_slice.packet_buffer.buffer_len;
    ++num_segments;
  }

  QUIC_DVLOG(2) << "Dispatched " << num_segments
                << " GRO segments with total size: " << bytes_processed;
  return num_segments;
}

}  // namespace quic
