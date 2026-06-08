// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/qbone/bonnet/icmp_reachable.h"

#include <netinet/ip6.h>

#include <memory>
#include <string>

#include "absl/container/node_hash_map.h"
#include "quiche/quic/core/io/quic_default_event_loop.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/qbone/platform/mock_kernel.h"

namespace quic::test {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

constexpr absl::string_view kInterfaceName = "qbone0";
constexpr char kSourceAddress[] = "fe80:1:2:3:4::1";
constexpr char kDestinationAddress[] = "fe80:4:3:2:1::1";

constexpr int kSendPort = 12345;

icmp6_hdr ParseIcmpHeader(const void* buf, size_t len) {
  QUICHE_CHECK_EQ(len, sizeof(icmp6_hdr));
  return *reinterpret_cast<const icmp6_hdr*>(
      &(reinterpret_cast<const char*>(buf))[0]);
}

class StatsInterface : public IcmpReachable::StatsInterface {
 public:
  void OnEvent(IcmpReachable::ReachableEvent event) override {
    switch (event.status) {
      case IcmpReachable::REACHABLE: {
        reachable_count_++;
        break;
      }
      case IcmpReachable::UNREACHABLE: {
        unreachable_count_++;
        break;
      }
    }
    current_source_ = event.source;
  }

  void OnReadError(int error) override { read_errors_[error]++; }

  void OnWriteError(int error) override { write_errors_[error]++; }

  bool HasWriteErrors() { return !write_errors_.empty(); }

  int WriteErrorCount(int error) { return write_errors_[error]; }

  bool HasReadErrors() { return !read_errors_.empty(); }

  int ReadErrorCount(int error) { return read_errors_[error]; }

  int reachable_count() { return reachable_count_; }

  int unreachable_count() { return unreachable_count_; }

  std::string current_source() { return current_source_; }

 private:
  int reachable_count_ = 0;
  int unreachable_count_ = 0;

  std::string current_source_{};

  absl::node_hash_map<int, int> read_errors_;
  absl::node_hash_map<int, int> write_errors_;
};

class IcmpReachableTest : public QuicTest {
 public:
  IcmpReachableTest()
      : event_loop_(GetDefaultEventLoop()->Create(QuicDefaultClock::Get())) {
    QUICHE_CHECK(source_.FromString(kSourceAddress));
    QUICHE_CHECK(destination_.FromString(kDestinationAddress));

    int pipe_fds[2];
    QUICHE_CHECK(pipe(pipe_fds) >= 0) << "pipe() failed";

    simulated_sock_fd_ = pipe_fds[0];
    simulated_sock_recv_fd_ = pipe_fds[1];
  }

  void SetFdExpectations() {
    InSequence seq;
    EXPECT_CALL(kernel_, if_nametoindex(_));
    EXPECT_CALL(kernel_, socket(_, _, _)).WillOnce(Return(simulated_sock_fd_));
    EXPECT_CALL(kernel_, bind(simulated_sock_fd_, _, _)).WillOnce(Return(0));

    EXPECT_CALL(kernel_, getsockname(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(
                            *reinterpret_cast<struct sockaddr*>(&send_socket_)),
                        Return(0)));

    EXPECT_CALL(kernel_, close(simulated_sock_fd_)).WillOnce([](int fd) {
      return close(fd);
    });
  }

 protected:
  QuicIpAddress source_;
  QuicIpAddress destination_;

  struct sockaddr_in6 send_socket_ = {.sin6_port = kSendPort};

  int simulated_sock_fd_;
  int simulated_sock_recv_fd_;

  StrictMock<MockKernel> kernel_;
  std::unique_ptr<QuicEventLoop> event_loop_;
  StatsInterface stats_;
};

TEST_F(IcmpReachableTest, SendsPings) {
  SetFdExpectations();
  IcmpReachable reachable(kInterfaceName, source_, destination_,
                          QuicTime::Delta::Zero(), &kernel_, event_loop_.get(),
                          &stats_);

  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(simulated_sock_fd_, _, _, _, _, _))
      .WillOnce([](int sockfd, const void* buf, size_t len, int flags,
                   const struct sockaddr* dest_addr, socklen_t addrlen) {
        auto icmp_header = ParseIcmpHeader(buf, len);
        EXPECT_EQ(icmp_header.icmp6_type, ICMP6_ECHO_REQUEST);
        EXPECT_EQ(icmp_header.icmp6_seq, 1);
        EXPECT_EQ(icmp_header.icmp6_id, kSendPort);
        return len;
      });

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_FALSE(stats_.HasWriteErrors());
}

TEST_F(IcmpReachableTest, HandlesUnreachableEvents) {
  SetFdExpectations();
  IcmpReachable reachable(kInterfaceName, source_, destination_,
                          QuicTime::Delta::Zero(), &kernel_, event_loop_.get(),
                          &stats_);

  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(simulated_sock_fd_, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly([](int sockfd, const void* buf, size_t len, int flags,
                         const struct sockaddr* dest_addr,
                         socklen_t addrlen) { return len; });

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.unreachable_count(), 0);

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_FALSE(stats_.HasWriteErrors());
  EXPECT_EQ(stats_.unreachable_count(), 1);
  EXPECT_EQ(stats_.current_source(), kNoSource);
}

TEST_F(IcmpReachableTest, HandlesReachableEvents) {
  SetFdExpectations();
  IcmpReachable reachable(kInterfaceName, source_, destination_,
                          QuicTime::Delta::Zero(), &kernel_, event_loop_.get(),
                          &stats_);

  ASSERT_TRUE(reachable.Init());

  icmp6_hdr last_request_hdr{};
  EXPECT_CALL(kernel_, sendto(simulated_sock_fd_, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly([&last_request_hdr](
                          int sockfd, const void* buf, size_t len, int flags,
                          const struct sockaddr* dest_addr, socklen_t addrlen) {
        last_request_hdr = ParseIcmpHeader(buf, len);
        return len;
      });

  sockaddr_in6 source_addr{};
  std::string packed_source = source_.ToPackedString();
  memcpy(&source_addr.sin6_addr, packed_source.data(), packed_source.size());

  EXPECT_CALL(kernel_, recvfrom(simulated_sock_fd_, _, _, _, _, _))
      .WillOnce([&source_addr](int sockfd, void* buf, size_t len, int flags,
                               struct sockaddr* src_addr, socklen_t* addrlen) {
        *reinterpret_cast<sockaddr_in6*>(src_addr) = source_addr;
        return read(sockfd, buf, len);
      });

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.reachable_count(), 0);

  icmp6_hdr response = last_request_hdr;
  response.icmp6_type = ICMP6_ECHO_REPLY;

  write(simulated_sock_recv_fd_, reinterpret_cast<const void*>(&response),
        sizeof(icmp6_hdr));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_FALSE(stats_.HasReadErrors());
  EXPECT_FALSE(stats_.HasWriteErrors());
  EXPECT_EQ(stats_.reachable_count(), 1);
  EXPECT_EQ(stats_.current_source(), source_.ToString());
}

TEST_F(IcmpReachableTest, HandlesWriteErrors) {
  SetFdExpectations();
  IcmpReachable reachable(kInterfaceName, source_, destination_,
                          QuicTime::Delta::Zero(), &kernel_, event_loop_.get(),
                          &stats_);

  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(simulated_sock_fd_, _, _, _, _, _))
      .WillOnce([](int sockfd, const void* buf, size_t len, int flags,
                   const struct sockaddr* dest_addr, socklen_t addrlen) {
        errno = EAGAIN;
        return 0;
      });

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.WriteErrorCount(EAGAIN), 1);
}

TEST_F(IcmpReachableTest, HandlesReadErrors) {
  SetFdExpectations();
  IcmpReachable reachable(kInterfaceName, source_, destination_,
                          QuicTime::Delta::Zero(), &kernel_, event_loop_.get(),
                          &stats_);

  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(simulated_sock_fd_, _, _, _, _, _))
      .WillOnce([](int sockfd, const void* buf, size_t len, int flags,
                   const struct sockaddr* dest_addr,
                   socklen_t addrlen) { return len; });

  EXPECT_CALL(kernel_, recvfrom(simulated_sock_fd_, _, _, _, _, _))
      .WillOnce([](int sockfd, void* buf, size_t len, int flags,
                   struct sockaddr* src_addr, socklen_t* addrlen) {
        errno = EIO;
        return -1;
      });

  icmp6_hdr response{};

  write(simulated_sock_recv_fd_, reinterpret_cast<const void*>(&response),
        sizeof(icmp6_hdr));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.reachable_count(), 0);
  EXPECT_EQ(stats_.ReadErrorCount(EIO), 1);
}

}  // namespace
}  // namespace quic::test
