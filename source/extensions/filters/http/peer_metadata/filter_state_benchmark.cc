// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/extensions/filters/http/peer_metadata/filter.h"

#include "source/common/formatter/substitution_formatter.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "extensions/common/metadata_object.h"

#include "test/common/stream_info/test_util.h"
#include "test/mocks/common.h"
#include "test/mocks/server/factory_context.h"
#include "test/test_common/utility.h"

#include "benchmark/benchmark.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace PeerMetadata {

namespace {

// Helper to create a WorkloadMetadataObject with realistic test data
std::unique_ptr<Istio::Common::WorkloadMetadataObject> makeWorkloadMetadata() {
  return std::make_unique<Istio::Common::WorkloadMetadataObject>(
      "sleep-v1-12345-abcde",                       // instance_name
      "cluster1",                                   // cluster_name
      "default",                                    // namespace_name
      "sleep-v1",                                   // workload_name
      "sleep",                                      // canonical_name
      "v1",                                         // canonical_revision
      "sleep",                                      // app_name
      "v1",                                         // app_version
      Istio::Common::WorkloadType::Pod,             // workload_type
      "spiffe://cluster.local/ns/default/sa/sleep", // identity
      "us-west1",                                   // region
      "us-west1-a"                                  // zone
  );
}

// Setup stream info with filter state for CEL access
void setupCelFilterState(Envoy::StreamInfo::StreamInfo& stream_info) {
  auto metadata = makeWorkloadMetadata();
  auto proto = metadata->serializeAsProto();

  // CEL access requires CelState wrapper under "downstream_peer" key
  auto cel_state =
      std::make_unique<Filters::Common::Expr::CelState>(FilterConfig::peerInfoPrototype());
  cel_state->setValue(absl::string_view(proto->SerializeAsString()));

  stream_info.filterState()->setData(
      std::string(Istio::Common::DownstreamPeer), std::move(cel_state),
      StreamInfo::FilterState::StateType::Mutable, StreamInfo::FilterState::LifeSpan::FilterChain);
}

// Setup stream info with filter state for FIELD access
void setupFieldFilterState(Envoy::StreamInfo::StreamInfo& stream_info) {
  auto metadata = makeWorkloadMetadata();

  // FIELD access uses WorkloadMetadataObject under "downstream_peer_obj" key
  stream_info.filterState()->setData(
      std::string(Istio::Common::DownstreamPeerObj), std::move(metadata),
      StreamInfo::FilterState::StateType::Mutable, StreamInfo::FilterState::LifeSpan::FilterChain);
}

} // namespace

// Benchmark CEL accessor for filter_state.downstream_peer.workload
// NOLINTNEXTLINE(readability-identifier-naming)
static void BM_FilterState_CEL(benchmark::State& state) {
  testing::NiceMock<MockTimeSystem> time_system;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  ScopedThreadLocalServerContextSetter server_context_setter(context.server_factory_context_);

  Envoy::TestStreamInfo stream_info(time_system);

  setupCelFilterState(stream_info);

  // CEL format: %CEL(filter_state.downstream_peer.workload)%
  const std::string format = "%CEL(filter_state.downstream_peer.workload)%";
  auto formatter = *Formatter::FormatterImpl::create(format, false);

  Formatter::Context formatter_context;
  size_t total_bytes_allocated = 0;

  for (auto _ : state) { // NOLINT
    std::string result = formatter->format(formatter_context, stream_info);
    // Count string allocation: capacity is usually result.size() rounded up to power of 2
    // For small strings like "sleep-v1", this is typically 16-32 bytes
    total_bytes_allocated += result.capacity();
    benchmark::DoNotOptimize(result);
  }

  // Report memory allocated per iteration
  state.SetBytesProcessed(total_bytes_allocated);
  state.SetLabel("alloc_per_iter=" + std::to_string(total_bytes_allocated / state.iterations()) +
                 "B");
}
BENCHMARK(BM_FilterState_CEL);

// Benchmark FIELD accessor for filter_state downstream_peer workload
// NOLINTNEXTLINE(readability-identifier-naming)
static void BM_FilterState_FIELD(benchmark::State& state) {
  testing::NiceMock<MockTimeSystem> time_system;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  ScopedThreadLocalServerContextSetter server_context_setter(context.server_factory_context_);

  Envoy::TestStreamInfo stream_info(time_system);

  setupFieldFilterState(stream_info);

  // FIELD format: %FILTER_STATE(downstream_peer_obj:FIELD:workload)%
  const std::string format = "%FILTER_STATE(downstream_peer_obj:FIELD:workload)%";
  auto formatter = *Formatter::FormatterImpl::create(format, false);

  Formatter::Context formatter_context;
  size_t total_bytes_allocated = 0;

  for (auto _ : state) { // NOLINT
    std::string result = formatter->format(formatter_context, stream_info);
    total_bytes_allocated += result.capacity();
    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(total_bytes_allocated);
  state.SetLabel("alloc_per_iter=" + std::to_string(total_bytes_allocated / state.iterations()) +
                 "B");
}
BENCHMARK(BM_FilterState_FIELD);

// Benchmark baseline - accessing filter state directly without formatter
// NOLINTNEXTLINE(readability-identifier-naming)
static void BM_FilterState_Direct(benchmark::State& state) {
  testing::NiceMock<MockTimeSystem> time_system;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  ScopedThreadLocalServerContextSetter server_context_setter(context.server_factory_context_);

  Envoy::TestStreamInfo stream_info(time_system);

  setupFieldFilterState(stream_info);

  size_t total_bytes_read = 0;

  for (auto _ : state) { // NOLINT
    const auto* obj =
        stream_info.filterState()->getDataReadOnly<Istio::Common::WorkloadMetadataObject>(
            std::string(Istio::Common::DownstreamPeerObj));
    if (obj) {
      // Direct access doesn't allocate - just reads the string_view
      total_bytes_read += obj->workload_name_.length();
    }
  }

  state.SetBytesProcessed(total_bytes_read);
  state.SetLabel("alloc_per_iter=0B (no allocation, direct access)");
  benchmark::DoNotOptimize(total_bytes_read);
}
BENCHMARK(BM_FilterState_Direct);

} // namespace PeerMetadata
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
