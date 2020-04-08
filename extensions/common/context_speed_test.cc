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
#include "common/stream_info/filter_state_impl.h"
#include "extensions/common/context.h"
#include "extensions/common/wasm/wasm_state.h"
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifdef NULL_PLUGIN
namespace Wasm {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf::util;
using namespace wasm::common;

constexpr absl::string_view node_metadata_json = R"###(
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

static void BM_GenericStructParser(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  auto bytes = metadata_struct.SerializeAsString();

  for (auto _ : state) {
    google::protobuf::Struct test_struct;
    test_struct.ParseFromArray(bytes.data(), bytes.size());
    benchmark::DoNotOptimize(test_struct);

    NodeInfo node_info;
    extractNodeMetadataGeneric(test_struct, &node_info);
    benchmark::DoNotOptimize(node_info);
  }
}
BENCHMARK(BM_GenericStructParser);

static void BM_CustomStructParser(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  auto bytes = metadata_struct.SerializeAsString();

  for (auto _ : state) {
    google::protobuf::Struct test_struct;
    test_struct.ParseFromArray(bytes.data(), bytes.size());
    benchmark::DoNotOptimize(test_struct);

    NodeInfo node_info;
    extractNodeMetadata(test_struct, &node_info);
    benchmark::DoNotOptimize(node_info);
  }
}
BENCHMARK(BM_CustomStructParser);

static void BM_MessageParser(benchmark::State& state) {
  NodeInfo node_info;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &node_info,
                      json_parse_options);
  auto bytes = node_info.SerializeAsString();

  for (auto _ : state) {
    NodeInfo test_info;
    test_info.ParseFromArray(bytes.data(), bytes.size());
    benchmark::DoNotOptimize(test_info);
  }
}
BENCHMARK(BM_MessageParser);

constexpr absl::string_view metadata_id_key =
    "envoy.wasm.metadata_exchange.downstream_id";
constexpr absl::string_view metadata_key =
    "envoy.wasm.metadata_exchange.downstream";
constexpr absl::string_view node_id = "test_pod.test_namespace";

static void setData(Envoy::StreamInfo::FilterStateImpl& filter_state,
                    absl::string_view key, absl::string_view value) {
  filter_state.setData(
      key, std::make_unique<Envoy::Extensions::Common::Wasm::WasmState>(value),
      Envoy::StreamInfo::FilterState::StateType::Mutable);
}

static const std::string& getData(
    Envoy::StreamInfo::FilterStateImpl& filter_state, absl::string_view key) {
  return filter_state
      .getDataReadOnly<Envoy::Extensions::Common::Wasm::WasmState>(key)
      .value();
}

typedef std::shared_ptr<const wasm::common::NodeInfo> NodeInfoPtr;

static void BM_ReadRawBytesWithCache(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  auto bytes = metadata_struct.SerializeAsString();
  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};
  setData(filter_state, metadata_id_key, node_id);
  setData(filter_state, metadata_key, bytes);

  std::unordered_map<std::string, NodeInfoPtr> cache;

  size_t size = 0;
  for (auto _ : state) {
    // lookup cache by key
    const std::string& peer_id = getData(filter_state, metadata_id_key);
    auto nodeinfo_it = cache.find(peer_id);
    const NodeInfo* node_info = nullptr;
    if (nodeinfo_it == cache.end()) {
      const std::string& bytes = getData(filter_state, metadata_key);
      google::protobuf::Struct test_struct;
      test_struct.ParseFromArray(bytes.data(), bytes.size());
      benchmark::DoNotOptimize(test_struct);

      auto node_info_ptr = std::make_shared<wasm::common::NodeInfo>();
      auto status = extractNodeMetadata(test_struct, node_info_ptr.get());
      node_info = node_info_ptr.get();
      cache.emplace(peer_id, std::move(node_info_ptr));
    } else {
      node_info = nodeinfo_it->second.get();
    }

    size += node_info->namespace_().size() + node_info->workload_name().size() +
            node_info->labels().at("app").size() +
            node_info->labels().at("version").size();
    benchmark::DoNotOptimize(size);
  }
}
BENCHMARK(BM_ReadRawBytesWithCache);

static void BM_ReadFlatBuffer(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  flatbuffers::FlatBufferBuilder fbb(1024);
  extractNodeFlatBuffer(metadata_struct, fbb);

  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};
  setData(
      filter_state, metadata_key,
      absl::string_view(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
                        fbb.GetSize()));

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
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
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
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
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

      flatbuffers::FlatBufferBuilder fbb;
      extractNodeFlatBuffer(test_struct, fbb);

      node_info =
          cache
              .emplace(node_id, std::string(reinterpret_cast<const char*>(
                                                fbb.GetBufferPointer()),
                                            fbb.GetSize()))
              .first->second;
    } else {
      node_info = nodeinfo_it->second;
    }

    setData(filter_state, metadata_id_key, node_id);
    setData(filter_state, metadata_key, node_info);
  }
}
BENCHMARK(BM_WriteFlatBufferWithCache);

}  // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Wasm
#endif

// Boilerplate main(), which discovers benchmarks in the same file and runs
// them.
int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
}
