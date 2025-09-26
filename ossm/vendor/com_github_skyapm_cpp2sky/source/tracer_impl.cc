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

#include "source/tracer_impl.h"

#include <chrono>
#include <thread>

#include "cpp2sky/exception.h"
#include "language-agent/ConfigurationDiscoveryService.pb.h"
#include "matchers/suffix_matcher.h"
#include "source/grpc_async_client_impl.h"
#include "spdlog/spdlog.h"

namespace cpp2sky {

using namespace spdlog;

TracerImpl::TracerImpl(const TracerConfig& config, CredentialsSharedPtr cred)
    : segment_factory_(config) {
  init(config, cred);
}

TracerImpl::TracerImpl(const TracerConfig& config,
                       TraceAsyncClientPtr async_client)
    : async_client_(std::move(async_client)), segment_factory_(config) {
  init(config, nullptr);
}

TracerImpl::~TracerImpl() {
  // Stop the reporter client.
  async_client_->resetClient();
}

TracingContextSharedPtr TracerImpl::newContext() {
  return segment_factory_.create();
}

TracingContextSharedPtr TracerImpl::newContext(SpanContextSharedPtr span) {
  return segment_factory_.create(span);
}

bool TracerImpl::report(TracingContextSharedPtr ctx) {
  if (!ctx || !ctx->readyToSend()) {
    return false;
  }

  if (!ctx->spans().empty()) {
    if (ignore_matcher_->match(ctx->spans().front()->operationName())) {
      return false;
    }
  }

  async_client_->sendMessage(ctx->createSegmentObject());
  return true;
}

void TracerImpl::init(const TracerConfig& config, CredentialsSharedPtr cred) {
  spdlog::set_level(spdlog::level::warn);

  if (async_client_ == nullptr) {
    if (config.protocol() != Protocol::GRPC) {
      throw TracerException("Only GRPC is supported.");
    }
    async_client_ = TraceAsyncClientImpl::createClient(
        config.address(), config.token(), nullptr, std::move(cred));
  }

  ignore_matcher_.reset(new SuffixMatcher(
      std::vector<std::string>(config.ignore_operation_name_suffix().begin(),
                               config.ignore_operation_name_suffix().end())));
}

TracerPtr createInsecureGrpcTracer(const TracerConfig& cfg) {
  return TracerPtr{new TracerImpl(cfg, grpc::InsecureChannelCredentials())};
}

}  // namespace cpp2sky
