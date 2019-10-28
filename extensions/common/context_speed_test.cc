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
#include "extensions/common/context.h"
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
