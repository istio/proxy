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

#include "src/envoy/tcp/alpn_proxy/alpn_proxy.h"
#include "common/buffer/buffer_impl.h"
#include "common/protobuf/protobuf.h"
#include "gmock/gmock.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "src/envoy/tcp/alpn_proxy/alpn_proxy_initial_header.h"
#include "test/mocks/local_info/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/protobuf/mocks.h"

using ::google::protobuf::util::MessageDifferencer;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Tcp {
namespace AlpnProxy {
namespace {

MATCHER_P(MapEq, rhs, "") { return MessageDifferencer::Equals(arg, rhs); }

void ConstructProxyHeaderData(::Envoy::Buffer::OwnedImpl& serialized_header,
                              Envoy::ProtobufWkt::Any& proxy_header,
                              AlpnProxyInitialHeader* initial_header) {
  std::string serialized_proxy_header = proxy_header.SerializeAsString();
  memset(initial_header, 0, sizeof(AlpnProxyInitialHeader));
  initial_header->magic = absl::ghtonl(AlpnProxyInitialHeader::magic_number);
  initial_header->data_size = absl::ghtonl(serialized_proxy_header.length());
  serialized_header.add(::Envoy::Buffer::OwnedImpl{
      absl::string_view(reinterpret_cast<const char*>(initial_header),
                        sizeof(AlpnProxyInitialHeader))});
  serialized_header.add(::Envoy::Buffer::OwnedImpl{serialized_proxy_header});
}

}  // namespace

class AlpnProxyFilterTest : public testing::Test {
 public:
  AlpnProxyFilterTest() { ENVOY_LOG_MISC(info, "test"); }

  void initialize() {
    config_ = std::make_shared<AlpnProxyConfig>(
        stat_prefix_, "istio2", "istio/metadata", FilterDirection::Downstream,
        scope_);
    filter_ = std::make_unique<AlpnProxyFilter>(config_, local_info_,
                                                validation_visitor_);
    filter_->initializeReadFilterCallbacks(read_filter_callbacks_);
    filter_->initializeWriteFilterCallbacks(write_filter_callbacks_);
    metadata_node_.set_id("test");
    EXPECT_CALL(read_filter_callbacks_.connection_, streamInfo())
        .WillRepeatedly(ReturnRef(stream_info_));
    EXPECT_CALL(local_info_, node()).WillRepeatedly(ReturnRef(metadata_node_));
  }

  void initializeStructValues() {
    (*details_value_.mutable_fields())["namespace"].set_string_value("default");
    (*details_value_.mutable_fields())["labels"].set_string_value(
        "{app, details}");

    (*productpage_value_.mutable_fields())["namespace"].set_string_value(
        "default");
    (*productpage_value_.mutable_fields())["labels"].set_string_value(
        "{app, productpage}");
  }

  Envoy::ProtobufWkt::Struct details_value_;
  Envoy::ProtobufWkt::Struct productpage_value_;
  AlpnProxyConfigSharedPtr config_;
  std::unique_ptr<AlpnProxyFilter> filter_;
  Stats::IsolatedStoreImpl scope_;
  std::string stat_prefix_{"test.alpnmetadata"};
  NiceMock<Network::MockReadFilterCallbacks> read_filter_callbacks_;
  NiceMock<Network::MockWriteFilterCallbacks> write_filter_callbacks_;
  Network::MockConnection connection_;
  NiceMock<LocalInfo::MockLocalInfo> local_info_;
  NiceMock<ProtobufMessage::MockValidationVisitor> validation_visitor_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  envoy::api::v2::core::Node metadata_node_;
};

TEST_F(AlpnProxyFilterTest, AlpnProxyFound) {
  initialize();
  initializeStructValues();

  auto node_metadata_map = metadata_node_.mutable_metadata()->mutable_fields();
  google::protobuf::Value& value = (*node_metadata_map)["istio/metadata"];
  (*value.mutable_struct_value()).CopyFrom(details_value_);

  EXPECT_CALL(read_filter_callbacks_.connection_, nextProtocol())
      .WillRepeatedly(Return("istio2"));
  EXPECT_CALL(stream_info_,
              setDynamicMetadata("filters.network.alpn_proxy.downstream",
                                 MapEq(details_value_)));
  EXPECT_CALL(stream_info_,
              setDynamicMetadata("filters.network.alpn_proxy.upstream",
                                 MapEq(productpage_value_)));

  ::Envoy::Buffer::OwnedImpl data;
  AlpnProxyInitialHeader initial_header;
  Envoy::ProtobufWkt::Any productpage_any_value;
  *productpage_any_value.mutable_type_url() =
      "type.googleapis.com/google.protobuf.Struct";
  *productpage_any_value.mutable_value() =
      productpage_value_.SerializeAsString();
  ConstructProxyHeaderData(data, productpage_any_value, &initial_header);
  ::Envoy::Buffer::OwnedImpl world{"world"};
  data.add(world);

  EXPECT_EQ(Envoy::Network::FilterStatus::Continue,
            filter_->onData(data, false));
  EXPECT_EQ(data.toString(), "world");

  EXPECT_EQ(0UL, config_->stats().initial_header_not_found_.value());
  EXPECT_EQ(0UL, config_->stats().header_not_found_.value());
  EXPECT_EQ(1UL, config_->stats().alpn_protocol_found_.value());
}

TEST_F(AlpnProxyFilterTest, AlpnProxyNotFound) {
  initialize();

  EXPECT_CALL(read_filter_callbacks_.connection_, nextProtocol())
      .WillRepeatedly(Return("istio"));

  ::Envoy::Buffer::OwnedImpl data{};
  EXPECT_EQ(Envoy::Network::FilterStatus::Continue,
            filter_->onData(data, false));
  EXPECT_EQ(1UL, config_->stats().alpn_protocol_not_found_.value());
}

}  // namespace AlpnProxy
}  // namespace Tcp
}  // namespace Envoy
