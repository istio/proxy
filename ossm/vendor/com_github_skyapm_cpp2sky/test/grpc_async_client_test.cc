// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "cpp2sky/internal/async_client.h"
#include "language-agent/Tracing.pb.h"
#include "source/grpc_async_client_impl.h"
#include "test/mocks.h"

namespace cpp2sky {

using testing::_;

struct TestStats {
  TestStats(uint64_t total, uint64_t dropped, uint64_t sent)
      : total_(total), dropped_(dropped), sent_(sent) {
    pending_ = total_ - dropped_ - sent_;
  }

  uint64_t total_{};
  uint64_t dropped_{};
  uint64_t sent_{};
  uint64_t pending_{};
};

class TestTraceAsyncClient : public TraceAsyncClientImpl {
 public:
  TestTraceAsyncClient(const std::string& address, const std::string& token,
                       TraceAsyncStreamFactoryPtr stream_factory,
                       CredentialsSharedPtr credentials)
      : TraceAsyncClientImpl(address, token, std::move(stream_factory),
                             std::move(credentials)) {}

  TestStats getTestStats() const {
    TestStats stats(messages_total_.load(), messages_dropped_.load(),
                    messages_sent_.load());
    return stats;
  }

  void notifyWriteEvent(bool success) { write_event_tag_.callback(success); }
  void notifyStartEvent(bool success) { basic_event_tag_.callback(success); }

  uint64_t bufferSize() const { return message_buffer_.size(); }
};

class TestTraceAsyncStreamFactory : public TraceAsyncStreamFactory {
 public:
  TestTraceAsyncStreamFactory(std::shared_ptr<MockTraceAsyncStream> mock_stream)
      : mock_stream_(mock_stream) {}

  class TestTraceAsyncStream : public TraceAsyncStream {
   public:
    TestTraceAsyncStream(std::shared_ptr<MockTraceAsyncStream> mock_stream)
        : mock_stream_(mock_stream) {}
    void sendMessage(TraceRequestType message) override {
      mock_stream_->sendMessage(std::move(message));
    }
    std::shared_ptr<MockTraceAsyncStream> mock_stream_;
  };

  TraceAsyncStreamPtr createStream(GrpcClientContextPtr, GrpcStub&,
                                   GrpcCompletionQueue&, AsyncEventTag&,
                                   AsyncEventTag&) override {
    return TraceAsyncStreamPtr{new TestTraceAsyncStream(mock_stream_)};
  }

  std::shared_ptr<MockTraceAsyncStream> mock_stream_;
};

class TraceAsyncClientImplTest : public testing::Test {
 public:
  TraceAsyncClientImplTest() {
    client_.reset(new TestTraceAsyncClient(
        address_, token_,
        TraceAsyncStreamFactoryPtr{
            new TestTraceAsyncStreamFactory(mock_stream_)},
        grpc::InsecureChannelCredentials()));
  }

  ~TraceAsyncClientImplTest() {
    client_->resetClient();
    client_.reset();
  }

 protected:
  std::string address_{"localhost:50051"};
  std::string token_{"token"};

  std::shared_ptr<MockTraceAsyncStream> mock_stream_ =
      std::make_shared<MockTraceAsyncStream>();

  std::unique_ptr<TestTraceAsyncClient> client_;
};

TEST_F(TraceAsyncClientImplTest, SendMessageTest) {
  skywalking::v3::SegmentObject fake_message;
  EXPECT_CALL(*mock_stream_, sendMessage(_)).Times(0);
  client_->sendMessage(fake_message);

  auto stats = client_->getTestStats();
  EXPECT_EQ(stats.total_, 1);
  EXPECT_EQ(stats.dropped_, 0);
  EXPECT_EQ(stats.sent_, 0);
  EXPECT_EQ(stats.pending_, 1);
  EXPECT_EQ(client_->bufferSize(), 1);

  client_->notifyStartEvent(false);

  sleep(1);  // wait for the event loop to process the event.

  // The stream is not ready, the message still in the buffer.
  stats = client_->getTestStats();
  EXPECT_EQ(stats.total_, 1);
  EXPECT_EQ(stats.dropped_, 0);
  EXPECT_EQ(stats.sent_, 0);
  EXPECT_EQ(stats.pending_, 1);
  EXPECT_EQ(client_->bufferSize(), 1);

  EXPECT_CALL(*mock_stream_, sendMessage(_));
  client_->notifyStartEvent(true);
  sleep(1);  // wait for the event loop to process the event.

  // The stream is ready, the message is popped and sent.
  // But before the collback is called, the stats is not updated.

  stats = client_->getTestStats();
  EXPECT_EQ(stats.total_, 1);
  EXPECT_EQ(stats.dropped_, 0);
  EXPECT_EQ(stats.sent_, 0);
  EXPECT_EQ(stats.pending_, 1);
  EXPECT_EQ(client_->bufferSize(), 0);

  client_->notifyWriteEvent(true);
  sleep(1);  // wait for the event loop to process the event.

  // The message is sent successfully.
  stats = client_->getTestStats();
  EXPECT_EQ(stats.total_, 1);
  EXPECT_EQ(stats.dropped_, 0);
  EXPECT_EQ(stats.sent_, 1);
  EXPECT_EQ(stats.pending_, 0);
  EXPECT_EQ(client_->bufferSize(), 0);

  // Send another message. This time the stream is ready and
  // previous message is sent successfully. So the new message
  // should be sent immediately.
  EXPECT_CALL(*mock_stream_, sendMessage(_));
  client_->sendMessage(fake_message);
  sleep(1);  // wait for the event loop to process the event.

  stats = client_->getTestStats();
  EXPECT_EQ(stats.total_, 2);
  EXPECT_EQ(stats.dropped_, 0);
  EXPECT_EQ(stats.sent_, 1);
  EXPECT_EQ(stats.pending_, 1);

  client_->notifyWriteEvent(true);
  sleep(1);  // wait for the event loop to process the event.

  stats = client_->getTestStats();
  EXPECT_EQ(stats.total_, 2);
  EXPECT_EQ(stats.dropped_, 0);
  EXPECT_EQ(stats.sent_, 2);
  EXPECT_EQ(stats.pending_, 0);
}

}  // namespace cpp2sky
