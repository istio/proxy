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
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kSourceAddress[] = "fe80:1:2:3:4::1";
constexpr char kDestinationAddress[] = "fe80:4:3:2:1::1";

constexpr int kFakeWriteFd = 0;

icmp6_hdr GetHeaderFromPacket(const void* buf, size_t len) {
  QUICHE_CHECK_GE(len, sizeof(ip6_hdr) + sizeof(icmp6_hdr));

  auto* buffer = reinterpret_cast<const char*>(buf);
  return *reinterpret_cast<const icmp6_hdr*>(&buffer[sizeof(ip6_hdr)]);
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

    read_fd_ = pipe_fds[0];
    read_src_fd_ = pipe_fds[1];
  }

  void SetFdExpectations() {
    InSequence seq;
    EXPECT_CALL(kernel_, socket(_, _, _)).WillOnce(Return(kFakeWriteFd));
    EXPECT_CALL(kernel_, bind(kFakeWriteFd, _, _)).WillOnce(Return(0));

    EXPECT_CALL(kernel_, socket(_, _, _)).WillOnce(Return(read_fd_));
    EXPECT_CALL(kernel_, bind(read_fd_, _, _)).WillOnce(Return(0));

    EXPECT_CALL(kernel_, setsockopt(read_fd_, SOL_ICMPV6, ICMP6_FILTER, _, _));

    EXPECT_CALL(kernel_, close(read_fd_)).WillOnce(Invoke([](int fd) {
      return close(fd);
    }));
  }

 protected:
  QuicIpAddress source_;
  QuicIpAddress destination_;

  int read_fd_;
  int read_src_fd_;

  StrictMock<MockKernel> kernel_;
  std::unique_ptr<QuicEventLoop> event_loop_;
  StatsInterface stats_;
};

TEST_F(IcmpReachableTest, SendsPings) {
  IcmpReachable reachable(source_, destination_, QuicTime::Delta::Zero(),
                          &kernel_, event_loop_.get(), &stats_);

  SetFdExpectations();
  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(kFakeWriteFd, _, _, _, _, _))
      .WillOnce(Invoke([](int sockfd, const void* buf, size_t len, int flags,
                          const struct sockaddr* dest_addr, socklen_t addrlen) {
        auto icmp_header = GetHeaderFromPacket(buf, len);
        EXPECT_EQ(icmp_header.icmp6_type, ICMP6_ECHO_REQUEST);
        EXPECT_EQ(icmp_header.icmp6_seq, 1);
        return len;
      }));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_FALSE(stats_.HasWriteErrors());
}

TEST_F(IcmpReachableTest, HandlesUnreachableEvents) {
  IcmpReachable reachable(source_, destination_, QuicTime::Delta::Zero(),
                          &kernel_, event_loop_.get(), &stats_);

  SetFdExpectations();
  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(kFakeWriteFd, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Invoke([](int sockfd, const void* buf, size_t len,
                                int flags, const struct sockaddr* dest_addr,
                                socklen_t addrlen) { return len; }));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.unreachable_count(), 0);

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_FALSE(stats_.HasWriteErrors());
  EXPECT_EQ(stats_.unreachable_count(), 1);
  EXPECT_EQ(stats_.current_source(), kNoSource);
}

TEST_F(IcmpReachableTest, HandlesReachableEvents) {
  IcmpReachable reachable(source_, destination_, QuicTime::Delta::Zero(),
                          &kernel_, event_loop_.get(), &stats_);

  SetFdExpectations();
  ASSERT_TRUE(reachable.Init());

  icmp6_hdr last_request_hdr{};
  EXPECT_CALL(kernel_, sendto(kFakeWriteFd, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(
          Invoke([&last_request_hdr](
                     int sockfd, const void* buf, size_t len, int flags,
                     const struct sockaddr* dest_addr, socklen_t addrlen) {
            last_request_hdr = GetHeaderFromPacket(buf, len);
            return len;
          }));

  sockaddr_in6 source_addr{};
  std::string packed_source = source_.ToPackedString();
  memcpy(&source_addr.sin6_addr, packed_source.data(), packed_source.size());

  EXPECT_CALL(kernel_, recvfrom(read_fd_, _, _, _, _, _))
      .WillOnce(
          Invoke([&source_addr](int sockfd, void* buf, size_t len, int flags,
                                struct sockaddr* src_addr, socklen_t* addrlen) {
            *reinterpret_cast<sockaddr_in6*>(src_addr) = source_addr;
            return read(sockfd, buf, len);
          }));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.reachable_count(), 0);

  icmp6_hdr response = last_request_hdr;
  response.icmp6_type = ICMP6_ECHO_REPLY;

  write(read_src_fd_, reinterpret_cast<const void*>(&response),
        sizeof(icmp6_hdr));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_FALSE(stats_.HasReadErrors());
  EXPECT_FALSE(stats_.HasWriteErrors());
  EXPECT_EQ(stats_.reachable_count(), 1);
  EXPECT_EQ(stats_.current_source(), source_.ToString());
}

TEST_F(IcmpReachableTest, HandlesWriteErrors) {
  IcmpReachable reachable(source_, destination_, QuicTime::Delta::Zero(),
                          &kernel_, event_loop_.get(), &stats_);

  SetFdExpectations();
  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(kFakeWriteFd, _, _, _, _, _))
      .WillOnce(Invoke([](int sockfd, const void* buf, size_t len, int flags,
                          const struct sockaddr* dest_addr, socklen_t addrlen) {
        errno = EAGAIN;
        return 0;
      }));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.WriteErrorCount(EAGAIN), 1);
}

TEST_F(IcmpReachableTest, HandlesReadErrors) {
  IcmpReachable reachable(source_, destination_, QuicTime::Delta::Zero(),
                          &kernel_, event_loop_.get(), &stats_);

  SetFdExpectations();
  ASSERT_TRUE(reachable.Init());

  EXPECT_CALL(kernel_, sendto(kFakeWriteFd, _, _, _, _, _))
      .WillOnce(Invoke([](int sockfd, const void* buf, size_t len, int flags,
                          const struct sockaddr* dest_addr,
                          socklen_t addrlen) { return len; }));

  EXPECT_CALL(kernel_, recvfrom(read_fd_, _, _, _, _, _))
      .WillOnce(Invoke([](int sockfd, void* buf, size_t len, int flags,
                          struct sockaddr* src_addr, socklen_t* addrlen) {
        errno = EIO;
        return -1;
      }));

  icmp6_hdr response{};

  write(read_src_fd_, reinterpret_cast<const void*>(&response),
        sizeof(icmp6_hdr));

  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  EXPECT_EQ(stats_.reachable_count(), 0);
  EXPECT_EQ(stats_.ReadErrorCount(EIO), 1);
}

}  // namespace
}  // namespace quic::test
