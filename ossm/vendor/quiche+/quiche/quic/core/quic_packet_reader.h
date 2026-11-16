// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to read incoming QUIC packets from the UDP socket.

#ifndef QUICHE_QUIC_CORE_QUIC_PACKET_READER_H_
#define QUICHE_QUIC_CORE_QUIC_PACKET_READER_H_

#include <cstddef>
#include <vector>

#include "absl/base/optimization.h"
#include "quiche/quic/core/quic_clock.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_process_packet_interface.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_udp_socket.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_socket_address.h"

namespace quic {

// Read in larger batches to minimize recvmmsg overhead.
inline constexpr int kNumPacketsPerReadMmsgCall = 16;

// The size of each packet buffer when reading UDP GRO trains: room for a whole
// train, which the kernel caps at 64 KB.
inline constexpr size_t kGroPacketBufferSize = 64 * 1024;

class QUICHE_EXPORT QuicPacketReader {
 public:
  struct QUICHE_EXPORT Options {
    // If true, each packet buffer has room for a whole UDP GRO train, and each
    // train read is dispatched as the individual packets in it. This takes
    // about 1 MB of buffers instead of about 32 KB. Only enable UDP GRO on a
    // socket (QuicUdpSocketApi::EnableUdpGro) if its reader has this set.
    bool enable_gro = false;
  };

  QuicPacketReader();
  explicit QuicPacketReader(Options options);
  QuicPacketReader(const QuicPacketReader&) = delete;
  QuicPacketReader& operator=(const QuicPacketReader&) = delete;

  virtual ~QuicPacketReader();

  // Reads a number of packets from the given fd, and then passes them off to
  // the PacketProcessInterface.  Returns true if there may be additional
  // packets available on the socket.
  // Populates |packets_dropped| if it is non-null and the socket is configured
  // to track dropped packets and some packets are read.
  // If the socket has timestamping enabled, the per packet timestamps will be
  // passed to the processor. Otherwise, |clock| will be used.
  virtual bool ReadAndDispatchPackets(int fd, int port, const QuicClock& clock,
                                      ProcessPacketInterface* processor,
                                      QuicPacketCount* packets_dropped);

  // Whether this reader reads UDP GRO trains. See Options::enable_gro.
  bool gro_enabled() const { return gro_enabled_; }

 private:
  // Return the self ip from |packet_info|.
  // For dual stack sockets, |packet_info| may contain both a v4 and a v6 ip, in
  // that case, |prefer_v6_ip| is used to determine which one is used as the
  // return value. If neither v4 nor v6 ip exists, return an uninitialized ip.
  static QuicIpAddress GetSelfIpFromPacketInfo(
      const QuicUdpPacketInfo& packet_info, bool prefer_v6_ip);

  // Passes the packet in |result| to |processor|, received at |now| on |port|.
  void DispatchPacket(const QuicUdpSocketApi::ReadPacketResult& result,
                      QuicTime now, int port,
                      ProcessPacketInterface* processor);

  struct QUICHE_EXPORT ReadBuffer {
    ABSL_CACHELINE_ALIGNED char
        control_buffer[kDefaultUdpPacketControlBufferSize];  // For ancillary
                                                             // data.
    ABSL_CACHELINE_ALIGNED char packet_buffer[kMaxIncomingPacketSize];
  };

  // A ReadBuffer with room for a whole UDP GRO train.
  struct QUICHE_EXPORT GroReadBuffer {
    ABSL_CACHELINE_ALIGNED char
        control_buffer[kDefaultUdpPacketControlBufferSize];
    ABSL_CACHELINE_ALIGNED char packet_buffer[kGroPacketBufferSize];
  };

  const bool gro_enabled_;
  QuicUdpSocketApi socket_api_;

  // Only one of these is allocated and used, depending on |gro_enabled_|.
  std::vector<ReadBuffer> read_buffers_;
  std::vector<GroReadBuffer> gro_read_buffers_;

  QuicUdpSocketApi::ReadPacketResults read_results_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QUIC_PACKET_READER_H_
