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

#include "metadata_to_peer_node.h"

#include "absl/strings/str_cat.h"
#include "envoy/network/listen_socket.h"
#include "envoy/stats/scope.h"
#include "envoy/stream_info/filter_state.h"
#include "extensions/common/context.h"
#include "extensions/common/util.h"
#include "src/envoy/common/metadata_object.h"

using namespace Envoy::Common;
using Envoy::Extensions::Filters::Common::Expr::CelState;

namespace Envoy {
namespace MetadataToPeerNode {

namespace {
const flatbuffers::DetachedBuffer convert(const WorkloadMetadataObject* obj) {
  flatbuffers::FlatBufferBuilder fbb;

  flatbuffers::Offset<flatbuffers::String> name, cluster, namespace_,
      workload_name;
  std::vector<flatbuffers::Offset<Wasm::Common::KeyVal>> labels;

  name = fbb.CreateString(obj->instanceName().data());
  namespace_ = fbb.CreateString(obj->namespaceName().data());
  cluster = fbb.CreateString(obj->clusterName().data());
  workload_name = fbb.CreateString(obj->workloadName().data());
  labels.push_back(Wasm::Common::CreateKeyVal(
      fbb, fbb.CreateString("service.istio.io/canonical-name"),
      fbb.CreateString(obj->canonicalName().data())));
  labels.push_back(Wasm::Common::CreateKeyVal(
      fbb, fbb.CreateString("service.istio.io/canonical-revision"),
      fbb.CreateString(obj->canonicalRevision().data())));
  // TODO: containers, ips, mesh id, cluster id ?
  auto labels_offset = fbb.CreateVectorOfSortedTables(&labels);
  Wasm::Common::FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_cluster_id(cluster);
  node.add_namespace_(namespace_);
  node.add_workload_name(workload_name);
  node.add_labels(labels_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return fbb.Release();
}
}  // namespace

Network::FilterStatus Filter::onAccept(Network::ListenerFilterCallbacks& cb) {
  ENVOY_LOG(trace, "metadata to peer: new connection accepted");

  StreamInfo::FilterState& filter_state = cb.filterState();

  const WorkloadMetadataObject* meta_obj =
      filter_state.getDataReadOnly<WorkloadMetadataObject>(
          WorkloadMetadataObject::kSourceMetadataObjectKey);

  if (meta_obj == nullptr) {
    ENVOY_LOG(trace, "metadata to peer: no metadata object found");
    return Network::FilterStatus::Continue;
  }

  // set the peer ID to the the key we want, then set the key with the FBB
  auto peer_id_state = std::make_unique<CelState>(Config::nodeIdPrototype());
  peer_id_state->setValue("connect_peer");
  filter_state.setData(
      absl::StrCat("wasm.",
                   toAbslStringView(Wasm::Common::kDownstreamMetadataIdKey)),
      std::move(peer_id_state), StreamInfo::FilterState::StateType::ReadOnly,
      StreamInfo::FilterState::LifeSpan::Connection);

  const auto fb = convert(meta_obj);
  auto peer_state = std::make_unique<CelState>(Config::nodeInfoPrototype());
  peer_state->setValue(
      absl::string_view(reinterpret_cast<const char*>(fb.data()), fb.size()));

  auto key = absl::StrCat(
      "wasm.", toAbslStringView(Wasm::Common::kDownstreamMetadataKey));
  filter_state.setData(key, std::move(peer_state),
                       StreamInfo::FilterState::StateType::ReadOnly,
                       StreamInfo::FilterState::LifeSpan::Connection);

  cb.socket().connectionInfoProvider().setSslConnection(meta_obj->ssl());

  ENVOY_LOG(
      trace,
      absl::StrCat(
          "metadata to peer: peer node set to filter state with key = ", key));

  return Network::FilterStatus::Continue;
}

Network::FilterStatus Filter::onData(Network::ListenerFilterBuffer&) {
  return Network::FilterStatus::Continue;
}

size_t Filter::maxReadBytes() const { return 0; }

}  // namespace MetadataToPeerNode
}  // namespace Envoy
