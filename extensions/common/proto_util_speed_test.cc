/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "benchmark/benchmark.h"
#include "extensions/common/metadata_object.h"
#include "extensions/common/node_info_generated.h"
#include "extensions/common/proto_util.h"
#include "extensions/common/util.h"
#include "google/protobuf/util/json_util.h"
#include "source/common/common/base64.h"
#include "source/common/stream_info/filter_state_impl.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "test/test_common/status_utility.h"

// WASM_PROLOG
#ifdef NULL_PLUGIN
namespace Wasm {
#endif // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf::util;

constexpr std::string_view node_metadata_json = R"###(
{
   "NAME":"test_pod",
   "NAMESPACE":"test_namespace",
   "LABELS": {
      "app": "productpage",
      "version": "v1",
      "pod-template-hash": "84975bc778"
   },
   "OWNER":"test_owner",
   "WORKLOAD_NAME":"test_workload",
   "PLATFORM_METADATA":{
      "gcp_project":"test_project",
      "gcp_cluster_location":"test_location",
      "gcp_cluster_name":"test_cluster"
   },
   "ISTIO_VERSION":"istio-1.4",
   "MESH_ID":"test-mesh"
}
)###";

constexpr std::string_view metadata_id_key = "envoy.wasm.metadata_exchange.downstream_id";
constexpr std::string_view metadata_key = "envoy.wasm.metadata_exchange.downstream";
constexpr std::string_view node_id = "test_pod.test_namespace";

static void setData(Envoy::StreamInfo::FilterStateImpl& filter_state, std::string_view key,
                    std::string_view value) {
  Envoy::Extensions::Filters::Common::Expr::CelStatePrototype prototype;
  auto state_ptr = std::make_unique<Envoy::Extensions::Filters::Common::Expr::CelState>(prototype);
  state_ptr->setValue(toAbslStringView(value));
  filter_state.setData(toAbslStringView(key), std::move(state_ptr),
                       Envoy::StreamInfo::FilterState::StateType::Mutable);
}

static const std::string& getData(Envoy::StreamInfo::FilterStateImpl& filter_state,
                                  std::string_view key) {
  return filter_state
      .getDataReadOnly<Envoy::Extensions::Filters::Common::Expr::CelState>(toAbslStringView(key))
      ->value();
}

static void BM_ReadFlatBuffer(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  ASSERT_OK(
      JsonStringToMessage(std::string(node_metadata_json), &metadata_struct, json_parse_options));
  auto out = extractNodeFlatBufferFromStruct(metadata_struct);

  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};
  setData(filter_state, metadata_key,
          std::string_view(reinterpret_cast<const char*>(out.data()), out.size()));

  size_t size = 0;
  for (auto _ : state) {
    auto buf = getData(filter_state, metadata_key);
    auto peer = flatbuffers::GetRoot<FlatNode>(buf.data());
    size += peer->workload_name()->size() + peer->namespace_()->size() +
            peer->labels()->LookupByKey("app")->value()->size() +
            peer->labels()->LookupByKey("version")->value()->size();
    benchmark::DoNotOptimize(size);
  }
}
BENCHMARK(BM_ReadFlatBuffer);

static void BM_WriteRawBytes(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  ASSERT_OK(
      JsonStringToMessage(std::string(node_metadata_json), &metadata_struct, json_parse_options));
  auto bytes = metadata_struct.SerializeAsString();
  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};

  for (auto _ : state) {
    setData(filter_state, metadata_id_key, node_id);
    setData(filter_state, metadata_key, bytes);
  }
}
BENCHMARK(BM_WriteRawBytes);

static void BM_WriteFlatBufferWithCache(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  ASSERT_OK(
      JsonStringToMessage(std::string(node_metadata_json), &metadata_struct, json_parse_options));
  auto bytes = metadata_struct.SerializeAsString();
  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};

  std::unordered_map<std::string, std::string> cache;

  for (auto _ : state) {
    // lookup cache by key
    auto nodeinfo_it = cache.find(std::string(node_id));
    std::string node_info;
    if (nodeinfo_it == cache.end()) {
      google::protobuf::Struct test_struct;
      test_struct.ParseFromArray(bytes.data(), bytes.size());
      benchmark::DoNotOptimize(test_struct);

      auto out = extractNodeFlatBufferFromStruct(test_struct);

      node_info =
          cache.emplace(node_id, std::string(reinterpret_cast<const char*>(out.data()), out.size()))
              .first->second;
    } else {
      node_info = nodeinfo_it->second;
    }

    setData(filter_state, metadata_id_key, node_id);
    setData(filter_state, metadata_key, node_info);
  }
}
BENCHMARK(BM_WriteFlatBufferWithCache);

constexpr std::string_view node_flatbuffer_json = R"###(
{
   "NAME":"test_pod",
   "NAMESPACE":"default",
   "CLUSTER_ID": "client-cluster",
   "LABELS": {
      "app": "productpage",
      "version": "v1",
      "service.istio.io/canonical-name": "productpage-v1",
      "service.istio.io/canonical-revision": "version-1"
   },
   "OWNER": "kubernetes://apis/apps/v1/namespaces/default/deployments/productpage-v1",
   "WORKLOAD_NAME":"productpage-v1"
}
)###";

// Measure decoding performance of x-envoy-peer-metadata.
static void BM_DecodeFlatBuffer(benchmark::State& state) {
  // Construct a header from sample value.
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  ASSERT_OK(
      JsonStringToMessage(std::string(node_flatbuffer_json), &metadata_struct, json_parse_options));
  std::string metadata_bytes;
  ::Wasm::Common::serializeToStringDeterministic(metadata_struct, &metadata_bytes);
  const std::string header_value =
      Envoy::Base64::encode(metadata_bytes.data(), metadata_bytes.size());

  size_t size = 0;
  for (auto _ : state) {
    auto bytes = Envoy::Base64::decodeWithoutPadding(header_value);
    google::protobuf::Struct metadata;
    metadata.ParseFromString(bytes);
    auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(metadata);
    size += fb.size();
    benchmark::DoNotOptimize(size);
  }
}
BENCHMARK(BM_DecodeFlatBuffer);

// Measure decoding performance of baggage.
static void BM_DecodeBaggage(benchmark::State& state) {
  // Construct a header from sample value.
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  ASSERT_OK(
      JsonStringToMessage(std::string(node_flatbuffer_json), &metadata_struct, json_parse_options));
  auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(metadata_struct);
  const auto& node = *flatbuffers::GetRoot<Wasm::Common::FlatNode>(fb.data());
  const std::string baggage = Istio::Common::convertFlatNodeToWorkloadMetadata(node).baggage();

  size_t size = 0;
  for (auto _ : state) {
    auto obj = Istio::Common::WorkloadMetadataObject::fromBaggage(baggage);
    size += obj.namespace_name_.size();
    benchmark::DoNotOptimize(size);
  }
}

BENCHMARK(BM_DecodeBaggage);

} // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
} // namespace Wasm
#endif
