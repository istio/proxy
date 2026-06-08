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

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "cpp2sky/config.pb.h"
#include "cpp2sky/internal/async_client.h"
#include "language-agent/Tracing.grpc.pb.h"
#include "source/utils/buffer.h"

namespace cpp2sky {

using CredentialsSharedPtr = std::shared_ptr<grpc::ChannelCredentials>;

using TraceGrpcStub =
    grpc::TemplatedGenericStub<TraceRequestType, TraceResponseType>;
using TraceReaderWriter =
    grpc::ClientAsyncReaderWriter<TraceRequestType, TraceResponseType>;
using TraceReaderWriterPtr = std::unique_ptr<TraceReaderWriter>;

class EventLoopThread {
 public:
  EventLoopThread() : thread_([this] { this->gogo(); }) {}
  ~EventLoopThread() { exit(); }

  grpc::CompletionQueue cq_;

  void exit() {
    if (!exited_) {
      exited_ = true;
      cq_.Shutdown();
      thread_.join();
    }
  }

 private:
  bool exited_{false};
  std::thread thread_;

  void gogo();
};

class TraceAsyncStreamImpl : public TraceAsyncStream {
 public:
  TraceAsyncStreamImpl(GrpcClientContextPtr client_ctx, TraceGrpcStub& stub,
                       GrpcCompletionQueue& cq, AsyncEventTag& basic_event_tag,
                       AsyncEventTag& write_event_tag);

  // AsyncStream
  void sendMessage(TraceRequestType message) override;

 private:
  GrpcClientContextPtr client_ctx_;
  TraceReaderWriterPtr request_writer_;

  AsyncEventTag& basic_event_tag_;
  AsyncEventTag& write_event_tag_;
};

class TraceAsyncStreamFactoryImpl : public TraceAsyncStreamFactory {
 public:
  TraceAsyncStreamFactoryImpl() = default;

  TraceAsyncStreamPtr createStream(GrpcClientContextPtr client_ctx,
                                   GrpcStub& stub, GrpcCompletionQueue& cq,
                                   AsyncEventTag& basic_event_tag,
                                   AsyncEventTag& write_event_tag) override;
};

class TraceAsyncClientImpl : public TraceAsyncClient {
 public:
  /**
   * Create a new GrpcAsyncSegmentReporterClient.
   *
   * @param address The address of the server.
   * @param token The optional token used to authenticate the client.
   * If non-empty token is provided, the client will send the token
   * to the server in the metadata.
   * @param cred The credentials for creating the channel.
   * @param factory The factory function to create the stream from the
   * request writer and event tags. In most cases, the default factory
   * should be used.
   */
  static std::unique_ptr<TraceAsyncClientImpl> createClient(
      const std::string& address, const std::string& token,
      TraceAsyncStreamFactoryPtr factory = nullptr,
      CredentialsSharedPtr cred = grpc::InsecureChannelCredentials());

  ~TraceAsyncClientImpl() override {
    if (!client_reset_) {
      resetClient();
    }
  }

  // AsyncClient
  void sendMessage(TraceRequestType message) override;
  void resetClient() override {
    // After this is called, no more events will be processed.
    client_reset_ = true;
    message_buffer_.clear();
    event_loop_.exit();
    resetStream();
  }

 protected:
  TraceAsyncClientImpl(
      const std::string& address, const std::string& token,
      TraceAsyncStreamFactoryPtr factory = nullptr,
      CredentialsSharedPtr cred = grpc::InsecureChannelCredentials());

  // Start or re-create the stream that used to send messages.
  void startStream();
  void resetStream();
  void markEventLoopIdle() { event_loop_idle_.store(true); }
  void sendMessageOnce();

  const std::string token_;
  TraceAsyncStreamFactoryPtr stream_factory_;
  TraceGrpcStub stub_;

  // This may be operated by multiple threads.
  std::atomic<uint64_t> messages_total_{0};
  std::atomic<uint64_t> messages_dropped_{0};
  std::atomic<uint64_t> messages_sent_{0};

  EventLoopThread event_loop_;
  std::atomic<bool> client_reset_{false};

  ValueBuffer<TraceRequestType> message_buffer_;

  AsyncEventTag basic_event_tag_;
  AsyncEventTag write_event_tag_;

  // The Write() of the stream could only be called once at a time
  // until the previous Write() is finished (callback is called).
  // Considering the complexity and the thread safety, we make sure
  // that all operations on the stream are done one by one.
  // This flag is used to indicate whether the event loop is idle
  // before we perform the next operation on the stream.
  //
  // Initially the value is false because the event loop will be
  // occupied by the first operation (startStream).
  std::atomic<bool> event_loop_idle_{false};

  TraceAsyncStreamPtr active_stream_;
};

}  // namespace cpp2sky
