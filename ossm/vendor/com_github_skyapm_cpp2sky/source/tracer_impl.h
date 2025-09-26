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

#include <thread>

#include "cpp2sky/internal/async_client.h"
#include "cpp2sky/internal/matcher.h"
#include "cpp2sky/tracer.h"
#include "language-agent/ConfigurationDiscoveryService.pb.h"
#include "source/grpc_async_client_impl.h"
#include "source/tracing_context_impl.h"
#include "source/utils/timer.h"

namespace cpp2sky {

using TracerRequestType = skywalking::v3::SegmentObject;
using TracerResponseType = skywalking::v3::Commands;

using CdsRequest = skywalking::v3::ConfigurationSyncRequest;
using CdsResponse = skywalking::v3::Commands;

class TracerImpl : public Tracer {
 public:
  TracerImpl(const TracerConfig& config, CredentialsSharedPtr credentials);
  TracerImpl(const TracerConfig& config, TraceAsyncClientPtr async_client);
  ~TracerImpl();

  TracingContextSharedPtr newContext() override;
  TracingContextSharedPtr newContext(SpanContextSharedPtr span) override;

  bool report(TracingContextSharedPtr ctx) override;

 private:
  void init(const TracerConfig& config, CredentialsSharedPtr cred);

  TraceAsyncClientPtr async_client_;
  TracingContextFactory segment_factory_;
  MatcherPtr ignore_matcher_;
};

}  // namespace cpp2sky
