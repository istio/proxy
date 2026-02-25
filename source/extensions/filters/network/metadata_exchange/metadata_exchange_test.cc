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

#include "source/extensions/filters/network/metadata_exchange/metadata_exchange.h"

#include "gmock/gmock.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "source/common/buffer/buffer_impl.h"
#include "source/common/protobuf/protobuf.h"
#include "source/extensions/filters/network/metadata_exchange/metadata_exchange_initial_header.h"
#include "test/mocks/local_info/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/protobuf/mocks.h"
#include "test/mocks/server/server_factory_context.h"

using ::google::protobuf::util::MessageDifferencer;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Tcp {
namespace MetadataExchange {
namespace {

MATCHER_P(MapEq, rhs, "") { return MessageDifferencer::Equals(arg, rhs); }

void ConstructProxyHeaderData(::Envoy::Buffer::OwnedImpl& serialized_header,
                              Envoy::Protobuf::Any& proxy_header,
                              MetadataExchangeInitialHeader* initial_header) {
  std::string serialized_proxy_header = proxy_header.SerializeAsString();
  memset(initial_header, 0, sizeof(MetadataExchangeInitialHeader));
  initial_header->magic = absl::ghtonl(MetadataExchangeInitialHeader::magic_number);
  initial_header->data_size = absl::ghtonl(serialized_proxy_header.length());
  serialized_header.add(::Envoy::Buffer::OwnedImpl{absl::string_view(
      reinterpret_cast<const char*>(initial_header), sizeof(MetadataExchangeInitialHeader))});
  serialized_header.add(::Envoy::Buffer::OwnedImpl{serialized_proxy_header});
}

} // namespace

class MetadataExchangeFilterTest : public testing::Test {
public:
  MetadataExchangeFilterTest() { ENVOY_LOG_MISC(info, "test"); }

  void initialize() { initialize(absl::flat_hash_set<std::string>()); }

  void initialize(absl::flat_hash_set<std::string> additional_labels) {
    config_ = std::make_shared<MetadataExchangeConfig>(
        stat_prefix_, "istio2", FilterDirection::Downstream, false, additional_labels, context_,
        *scope_.rootScope());
    filter_ = std::make_unique<MetadataExchangeFilter>(config_, local_info_);
    filter_->initializeReadFilterCallbacks(read_filter_callbacks_);
    filter_->initializeWriteFilterCallbacks(write_filter_callbacks_);
    metadata_node_.set_id("test");
    auto node_metadata_map = metadata_node_.mutable_metadata()->mutable_fields();
    (*node_metadata_map)["namespace"].set_string_value("default");
    (*node_metadata_map)["labels"].set_string_value("{app, details}");
    EXPECT_CALL(read_filter_callbacks_.connection_, streamInfo())
        .WillRepeatedly(ReturnRef(stream_info_));
    EXPECT_CALL(local_info_, node()).WillRepeatedly(ReturnRef(metadata_node_));
  }

  void initializeStructValues() {
    (*details_value_.mutable_fields())["namespace"].set_string_value("default");
    (*details_value_.mutable_fields())["labels"].set_string_value("{app, details}");

    (*productpage_value_.mutable_fields())["namespace"].set_string_value("default");
    (*productpage_value_.mutable_fields())["labels"].set_string_value("{app, productpage}");
  }

  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  Envoy::Protobuf::Struct details_value_;
  Envoy::Protobuf::Struct productpage_value_;
  MetadataExchangeConfigSharedPtr config_;
  std::unique_ptr<MetadataExchangeFilter> filter_;
  Stats::IsolatedStoreImpl scope_;
  std::string stat_prefix_{"test.metadataexchange"};
  NiceMock<Network::MockReadFilterCallbacks> read_filter_callbacks_;
  NiceMock<Network::MockWriteFilterCallbacks> write_filter_callbacks_;
  Network::MockConnection connection_;
  NiceMock<LocalInfo::MockLocalInfo> local_info_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  envoy::config::core::v3::Node metadata_node_;
};

TEST_F(MetadataExchangeFilterTest, MetadataExchangeFound) {
  initialize();
  initializeStructValues();

  EXPECT_CALL(read_filter_callbacks_.connection_, nextProtocol()).WillRepeatedly(Return("istio2"));

  ::Envoy::Buffer::OwnedImpl data;
  MetadataExchangeInitialHeader initial_header;
  Envoy::Protobuf::Any productpage_any_value;
  productpage_any_value.set_type_url("type.googleapis.com/google.protobuf.Struct");
  *productpage_any_value.mutable_value() = productpage_value_.SerializeAsString();
  ConstructProxyHeaderData(data, productpage_any_value, &initial_header);
  ::Envoy::Buffer::OwnedImpl world{"world"};
  data.add(world);

  EXPECT_EQ(Envoy::Network::FilterStatus::Continue, filter_->onData(data, false));
  EXPECT_EQ(data.toString(), "world");

  EXPECT_EQ(0UL, config_->stats().initial_header_not_found_.value());
  EXPECT_EQ(0UL, config_->stats().header_not_found_.value());
  EXPECT_EQ(1UL, config_->stats().alpn_protocol_found_.value());
}

TEST_F(MetadataExchangeFilterTest, MetadataExchangeAdditionalLabels) {
  initialize({"role"});
  initializeStructValues();

  EXPECT_CALL(read_filter_callbacks_.connection_, nextProtocol()).WillRepeatedly(Return("istio2"));

  ::Envoy::Buffer::OwnedImpl data;
  MetadataExchangeInitialHeader initial_header;
  Envoy::Protobuf::Any productpage_any_value;
  productpage_any_value.set_type_url("type.googleapis.com/google.protobuf.Struct");
  *productpage_any_value.mutable_value() = productpage_value_.SerializeAsString();
  ConstructProxyHeaderData(data, productpage_any_value, &initial_header);
  ::Envoy::Buffer::OwnedImpl world{"world"};
  data.add(world);

  EXPECT_EQ(Envoy::Network::FilterStatus::Continue, filter_->onData(data, false));
  EXPECT_EQ(data.toString(), "world");

  EXPECT_EQ(0UL, config_->stats().initial_header_not_found_.value());
  EXPECT_EQ(0UL, config_->stats().header_not_found_.value());
  EXPECT_EQ(1UL, config_->stats().alpn_protocol_found_.value());
}

TEST_F(MetadataExchangeFilterTest, MetadataExchangeNotFound) {
  initialize();

  EXPECT_CALL(read_filter_callbacks_.connection_, nextProtocol()).WillRepeatedly(Return("istio"));

  ::Envoy::Buffer::OwnedImpl data{};
  EXPECT_EQ(Envoy::Network::FilterStatus::Continue, filter_->onData(data, false));
  EXPECT_EQ(1UL, config_->stats().alpn_protocol_not_found_.value());
}

// Regression test for https://github.com/istio/istio/issues/59183
// Verifies that TCP metadata exchange stores peer info under both keys:
// - downstream_peer (CelState for CEL expressions)
// - downstream_peer_obj (WorkloadMetadataObject for FIELD accessor and istio_stats peerInfoRead)
TEST_F(MetadataExchangeFilterTest, MetadataExchangeStoresBothFilterStateKeys) {
  initialize();

  EXPECT_CALL(read_filter_callbacks_.connection_, nextProtocol()).WillRepeatedly(Return("istio2"));

  // Create inner peer metadata struct with workload info
  // Field names must match constants in metadata_object.h
  Envoy::Protobuf::Struct inner_metadata;
  (*inner_metadata.mutable_fields())["NAMESPACE"].set_string_value("test-ns");
  (*inner_metadata.mutable_fields())["WORKLOAD_NAME"].set_string_value("test-workload");
  (*inner_metadata.mutable_fields())["CLUSTER_ID"].set_string_value("test-cluster");
  // App name comes from LABELS.app (nested struct)
  Envoy::Protobuf::Struct labels;
  (*labels.mutable_fields())["app"].set_string_value("test-app");
  *(*inner_metadata.mutable_fields())["LABELS"].mutable_struct_value() = labels;

  // Wrap in outer struct with x-envoy-peer-metadata key (required by filter)
  Envoy::Protobuf::Struct outer_metadata;
  *(*outer_metadata.mutable_fields())["x-envoy-peer-metadata"].mutable_struct_value() =
      inner_metadata;

  ::Envoy::Buffer::OwnedImpl data;
  MetadataExchangeInitialHeader initial_header;
  Envoy::Protobuf::Any peer_any_value;
  peer_any_value.set_type_url("type.googleapis.com/google.protobuf.Struct");
  *peer_any_value.mutable_value() = outer_metadata.SerializeAsString();
  ConstructProxyHeaderData(data, peer_any_value, &initial_header);
  data.add(::Envoy::Buffer::OwnedImpl{"payload"});

  EXPECT_EQ(Envoy::Network::FilterStatus::Continue, filter_->onData(data, false));
  EXPECT_EQ(data.toString(), "payload");

  // Verify CelState is stored under downstream_peer (for CEL expression compatibility)
  const auto* cel_state = stream_info_.filterState()
                              ->getDataReadOnly<Envoy::Extensions::Filters::Common::Expr::CelState>(
                                  Istio::Common::DownstreamPeer);
  ASSERT_NE(cel_state, nullptr) << "CelState should be stored under downstream_peer";

  // Verify WorkloadMetadataObject is stored under downstream_peer_obj
  // This is critical for istio_stats peerInfoRead() to detect peer metadata
  const auto* peer_obj =
      stream_info_.filterState()->getDataReadOnly<Istio::Common::WorkloadMetadataObject>(
          Istio::Common::DownstreamPeerObj);
  ASSERT_NE(peer_obj, nullptr)
      << "WorkloadMetadataObject should be stored under downstream_peer_obj";

  // Verify the WorkloadMetadataObject has correct data
  EXPECT_EQ("test-ns", peer_obj->namespace_name_);
  EXPECT_EQ("test-workload", peer_obj->workload_name_);
  EXPECT_EQ("test-cluster", peer_obj->cluster_name_);
  EXPECT_EQ("test-app", peer_obj->app_name_);
}

} // namespace MetadataExchange
} // namespace Tcp
} // namespace Envoy
