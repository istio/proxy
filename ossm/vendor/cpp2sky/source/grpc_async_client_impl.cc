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

#include "source/grpc_async_client_impl.h"

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <thread>

#include "absl/strings/string_view.h"
#include "cpp2sky/exception.h"
#include "cpp2sky/internal/async_client.h"
#include "spdlog/spdlog.h"

namespace cpp2sky {

namespace {

static constexpr uint32_t MaxPendingMessagesSize = 1024;

static std::string AuthenticationKey = "authentication";

static std::string TraceCollectMethod = "/TraceSegmentReportService/collect";

}  // namespace

using namespace spdlog;

void EventLoopThread::gogo() {
  while (true) {
    void* got_tag{nullptr};
    bool ok{false};

    // true if got an event from the queue or false
    // if the queue is fully drained and is shutdown.
    const bool status = cq_.Next(&got_tag, &ok);
    if (!status) {
      assert(got_tag == nullptr);
      assert(!ok);
      info("[Reporter] Completion queue is drained and is shutdown.");
      break;
    }

    assert(got_tag != nullptr);

    // The lifetime of the tag is managed by the caller.
    auto* tag = static_cast<AsyncEventTag*>(got_tag);
    tag->callback(ok);
  }
}

TraceAsyncStreamImpl::TraceAsyncStreamImpl(GrpcClientContextPtr client_ctx,
                                           TraceGrpcStub& stub,
                                           GrpcCompletionQueue& cq,
                                           AsyncEventTag& basic_event_tag,
                                           AsyncEventTag& write_event_tag)
    : client_ctx_(std::move(client_ctx)),
      basic_event_tag_(basic_event_tag),
      write_event_tag_(write_event_tag) {
  if (client_ctx_ == nullptr) {
    client_ctx_.reset(new grpc::ClientContext());
  }

  request_writer_ =
      stub.PrepareCall(client_ctx_.get(), TraceCollectMethod, &cq);
  request_writer_->StartCall(reinterpret_cast<void*>(&basic_event_tag_));
}

void TraceAsyncStreamImpl::sendMessage(TraceRequestType message) {
  request_writer_->Write(message, reinterpret_cast<void*>(&write_event_tag_));
}

TraceAsyncStreamPtr TraceAsyncStreamFactoryImpl::createStream(
    GrpcClientContextPtr client_ctx, TraceGrpcStub& stub,
    GrpcCompletionQueue& cq, AsyncEventTag& basic_event_tag,
    AsyncEventTag& write_event_tag) {
  return TraceAsyncStreamPtr{new TraceAsyncStreamImpl(
      std::move(client_ctx), stub, cq, basic_event_tag, write_event_tag)};
}

std::unique_ptr<TraceAsyncClientImpl> TraceAsyncClientImpl::createClient(
    const std::string& address, const std::string& token,
    TraceAsyncStreamFactoryPtr factory, CredentialsSharedPtr cred) {
  return std::unique_ptr<TraceAsyncClientImpl>{new TraceAsyncClientImpl(
      address, token, std::move(factory), std::move(cred))};
}

TraceAsyncClientImpl::TraceAsyncClientImpl(const std::string& address,
                                           const std::string& token,
                                           TraceAsyncStreamFactoryPtr factory,
                                           CredentialsSharedPtr cred)
    : token_(token),
      stream_factory_(std::move(factory)),
      stub_(grpc::CreateChannel(address, cred)) {
  basic_event_tag_.callback = [this](bool ok) {
    if (client_reset_) {
      return;
    }

    if (ok) {
      trace("[Reporter] Stream event success.", fmt::ptr(this));

      // Mark event loop as idle because the previous Write() or
      // other operations are successful.
      markEventLoopIdle();

      sendMessageOnce();
      return;
    } else {
      trace("[Reporter] Stream event failure.", fmt::ptr(this));

      // Do not mark event loop as idle because the previous Write()
      // or other operations are failed. The event loop should keep
      // running to process the re-creation of the stream.
      assert(event_loop_idle_.load() == false);
      // Reset stream and try to create a new one.
      startStream();
    }
  };

  write_event_tag_.callback = [this](bool ok) {
    if (ok) {
      trace("[Reporter] Stream {} message sending success.", fmt::ptr(this));
      messages_sent_++;
    } else {
      trace("[Reporter] Stream {} message sending failure.", fmt::ptr(this));
      messages_dropped_++;
    }
    // Delegate the event to basic_event_tag_ to trigger the next task or
    // reset the stream.
    basic_event_tag_.callback(ok);
  };

  // If the factory is not provided, use the default one.
  if (stream_factory_ == nullptr) {
    stream_factory_.reset(new TraceAsyncStreamFactoryImpl());
  }

  startStream();
}

void TraceAsyncClientImpl::sendMessageOnce() {
  bool expect_idle = true;
  if (event_loop_idle_.compare_exchange_strong(expect_idle, false)) {
    assert(active_stream_ != nullptr);

    auto opt_message = message_buffer_.pop_front();
    if (!opt_message.has_value()) {
      // No message to send, mark event loop as idle.
      markEventLoopIdle();
      return;
    }

    active_stream_->sendMessage(std::move(opt_message).value());
  }
}

void TraceAsyncClientImpl::startStream() {
  if (active_stream_ != nullptr) {
    resetStream();  // Reset stream before creating a new one.
  }

  // Create the unique client context for the new stream.
  // Each stream should have its own context.
  auto client_ctx = GrpcClientContextPtr{new grpc::ClientContext()};
  if (!token_.empty()) {
    client_ctx->AddMetadata(AuthenticationKey, token_);
  }

  active_stream_ = stream_factory_->createStream(
      std::move(client_ctx), stub_, event_loop_.cq_, basic_event_tag_,
      write_event_tag_);

  info("[Reporter] Stream {} has created.", fmt::ptr(active_stream_.get()));
}

void TraceAsyncClientImpl::resetStream() {
  info("[Reporter] Stream {} has deleted.", fmt::ptr(active_stream_.get()));
  active_stream_.reset();
}

void TraceAsyncClientImpl::sendMessage(TraceRequestType message) {
  messages_total_++;

  const size_t pending = message_buffer_.size();
  if (pending > MaxPendingMessagesSize) {
    info("[Reporter] pending message overflow and drop message");
    messages_dropped_++;
    return;
  }
  message_buffer_.push_back(std::move(message));

  sendMessageOnce();
}

}  // namespace cpp2sky
