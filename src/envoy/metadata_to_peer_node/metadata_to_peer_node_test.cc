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
#include "extensions/common/metadata_object.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/protobuf/protobuf.h"
#include "src/envoy/metadata_to_peer_node/config/metadata_to_peer_node.pb.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

using Envoy::Extensions::Filters::Common::Expr::CelState;
using Istio::Common::WorkloadMetadataObject;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace MetadataToPeerNode {

class MetadataToPeerNodeFilterTest : public testing::Test {
 public:
  MetadataToPeerNodeFilterTest() { ENVOY_LOG_MISC(info, "test"); }

 public:
  void initializeFilter() {
    istio::telemetry::metadatatopeernode::v1::Config proto_config;
    config_ = std::make_shared<Config>(proto_config);
    filter_ = std::make_shared<Filter>(config_);

    filter_state_ = std::make_shared<StreamInfo::FilterStateImpl>(
        StreamInfo::FilterState::LifeSpan::Request);

    ON_CALL(callbacks_, filterState())
        .WillByDefault(Invoke(
            [&]() -> StreamInfo::FilterState& { return *filter_state_; }));
  }

  ConfigSharedPtr config_;
  std::shared_ptr<Filter> filter_;
  std::shared_ptr<StreamInfo::FilterState> filter_state_;

  NiceMock<Network::MockListenerFilterCallbacks> callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
};

TEST_F(MetadataToPeerNodeFilterTest, SetPeerInfoTest) {
  initializeFilter();
  auto baggage =
      absl::StrCat("k8s.cluster.name=foo-cluster,k8s.namespace.name=default,",
                   "k8s.deployment.name=foo-deploy,service.name=foo-service,",
                   "service.version=v1alpha3");
  auto obj = std::make_shared<WorkloadMetadataObject>(
      WorkloadMetadataObject::fromBaggage(baggage));

  filter_state_->setData(Istio::Common::kSourceMetadataObjectKey, obj,
                         StreamInfo::FilterState::StateType::ReadOnly,
                         StreamInfo::FilterState::LifeSpan::Request);

  EXPECT_EQ(filter_->onAccept(callbacks_), Network::FilterStatus::Continue);

  auto peer_id_key = absl::StrCat(
      "wasm.", toAbslStringView(Wasm::Common::kDownstreamMetadataIdKey));
  EXPECT_TRUE(filter_state_->hasDataWithName(peer_id_key));

  auto id = filter_state_->getDataReadOnly<CelState>(peer_id_key);
  EXPECT_EQ("connect_peer", id->value());

  auto peer_fbb_key = absl::StrCat(
      "wasm.", toAbslStringView(Wasm::Common::kDownstreamMetadataKey));
  EXPECT_TRUE(filter_state_->hasDataWithName(peer_fbb_key));

  auto peer_fbb_cel = filter_state_->getDataReadOnly<CelState>(peer_fbb_key);
  absl::string_view fbb_value = peer_fbb_cel->value();

  auto& peer = *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(
      reinterpret_cast<const uint8_t*>(fbb_value.data()));

  EXPECT_EQ(peer.namespace_()->string_view(), "default");
  EXPECT_EQ(peer.workload_name()->string_view(), "foo-deploy");
  EXPECT_EQ(peer.cluster_id()->string_view(), "foo-cluster");

  auto peer_labels = peer.labels();
  auto canonical_name = peer_labels->LookupByKey(
      ::Wasm::Common::kCanonicalServiceLabelName.data());
  auto canonical_rev = peer_labels->LookupByKey(
      ::Wasm::Common::kCanonicalServiceRevisionLabelName.data());
  EXPECT_EQ(canonical_name->value()->string_view(), "foo-service");
  EXPECT_EQ(canonical_rev->value()->string_view(), "v1alpha3");
}

TEST_F(MetadataToPeerNodeFilterTest, SetPeerInfoNoClusterTest) {
  initializeFilter();
  auto baggage =
      absl::StrCat("k8s.namespace.name=default,k8s.deployment.name=foo-deploy,",
                   "service.name=foo-service,service.version=v1alpha3");
  auto obj = std::make_shared<WorkloadMetadataObject>(
      WorkloadMetadataObject::fromBaggage(baggage));

  filter_state_->setData(Istio::Common::kSourceMetadataObjectKey, obj,
                         StreamInfo::FilterState::StateType::ReadOnly,
                         StreamInfo::FilterState::LifeSpan::Request);

  EXPECT_EQ(filter_->onAccept(callbacks_), Network::FilterStatus::Continue);

  auto peer_id_key = absl::StrCat(
      "wasm.", toAbslStringView(Wasm::Common::kDownstreamMetadataIdKey));
  EXPECT_TRUE(filter_state_->hasDataWithName(peer_id_key));

  auto id = filter_state_->getDataReadOnly<CelState>(peer_id_key);
  EXPECT_EQ("connect_peer", id->value());

  auto peer_fbb_key = absl::StrCat(
      "wasm.", toAbslStringView(Wasm::Common::kDownstreamMetadataKey));
  EXPECT_TRUE(filter_state_->hasDataWithName(peer_fbb_key));

  auto peer_fbb_cel = filter_state_->getDataReadOnly<CelState>(peer_fbb_key);
  absl::string_view fbb_value = peer_fbb_cel->value();

  auto& peer = *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(
      reinterpret_cast<const uint8_t*>(fbb_value.data()));

  EXPECT_EQ(peer.namespace_()->string_view(), "default");
  EXPECT_EQ(peer.workload_name()->string_view(), "foo-deploy");
  EXPECT_EQ(peer.cluster_id()->string_view(), "");
}

}  // namespace MetadataToPeerNode
}  // namespace Envoy
