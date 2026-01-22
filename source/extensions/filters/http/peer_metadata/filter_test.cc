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

#include "source/common/network/address_impl.h"
#include "test/common/stream_info/test_util.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/mocks/server/factory_context.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using Istio::Common::WorkloadMetadataObject;
using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace PeerMetadata {
namespace {

class MockSingletonManager : public Singleton::Manager {
public:
  MockSingletonManager() {}
  ~MockSingletonManager() override {}
  MOCK_METHOD(Singleton::InstanceSharedPtr, get,
              (const std::string& name, Singleton::SingletonFactoryCb cb, bool pin));
};
class MockWorkloadMetadataProvider
    : public Extensions::Common::WorkloadDiscovery::WorkloadMetadataProvider,
      public Singleton::Instance {
public:
  MockWorkloadMetadataProvider() {}
  ~MockWorkloadMetadataProvider() override {}
  MOCK_METHOD(std::optional<WorkloadMetadataObject>, GetMetadata,
              (const Network::Address::InstanceConstSharedPtr& address));
};

class PeerMetadataTest : public testing::Test {
protected:
  PeerMetadataTest() {
    ON_CALL(context_.server_factory_context_, singletonManager())
        .WillByDefault(ReturnRef(singleton_manager_));
    metadata_provider_ = std::make_shared<NiceMock<MockWorkloadMetadataProvider>>();
    ON_CALL(singleton_manager_, get(HasSubstr("workload_metadata_provider"), _, _))
        .WillByDefault(Return(metadata_provider_));
  }
  void initialize(const std::string& yaml_config) {
    TestUtility::loadFromYaml(yaml_config, config_);
    FilterConfigFactory factory;
    Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(config_, "", context_).value();
    Http::MockFilterChainFactoryCallbacks filter_callback;
    ON_CALL(filter_callback, addStreamFilter(_)).WillByDefault(testing::SaveArg<0>(&filter_));
    EXPECT_CALL(filter_callback, addStreamFilter(_));
    cb(filter_callback);
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(testing::ReturnRef(stream_info_));
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers_, true));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers_, true));
  }
  void checkNoPeer(bool downstream) {
    EXPECT_FALSE(stream_info_.filterState()->hasDataWithName(
        downstream ? Istio::Common::DownstreamPeer : Istio::Common::UpstreamPeer));
  }
  void checkPeerNamespace(bool downstream, const std::string& expected) {
    const auto* cel_state =
        stream_info_.filterState()
            ->getDataReadOnly<Envoy::Extensions::Filters::Common::Expr::CelState>(
                downstream ? Istio::Common::DownstreamPeer : Istio::Common::UpstreamPeer);
    Protobuf::Struct obj;
    ASSERT_TRUE(obj.ParseFromString(cel_state->value().data()));
    EXPECT_EQ(expected, extractString(obj, "namespace"));
  }

  absl::string_view extractString(const Protobuf::Struct& metadata, absl::string_view key) {
    const auto& it = metadata.fields().find(key);
    if (it == metadata.fields().end()) {
      return {};
    }
    return it->second.string_value();
  }

  void checkShared(bool expected) {
    EXPECT_EQ(expected,
              stream_info_.filterState()->objectsSharedWithUpstreamConnection()->size() > 0);
  }
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<MockSingletonManager> singleton_manager_;
  std::shared_ptr<NiceMock<MockWorkloadMetadataProvider>> metadata_provider_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  Http::TestRequestHeaderMapImpl request_headers_;
  Http::TestResponseHeaderMapImpl response_headers_;
  io::istio::http::peer_metadata::Config config_;
  Http::StreamFilterSharedPtr filter_;
};

TEST_F(PeerMetadataTest, None) {
  initialize("{}");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamXDSNone) {
  EXPECT_CALL(*metadata_provider_, GetMetadata(_)).WillRepeatedly(Return(std::nullopt));
  initialize(R"EOF(
    downstream_discovery:
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamXDS) {
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                                   "v1alpha3", "", "", Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "127.0.0.1")) {
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    downstream_discovery:
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "default");
  checkNoPeer(false);
  checkShared(false);
}

TEST_F(PeerMetadataTest, UpstreamXDS) {
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "foo", "foo", "foo-service",
                                   "v1alpha3", "", "", Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "10.0.0.1")) {
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    upstream_discovery:
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "foo");
}

TEST_F(PeerMetadataTest, UpstreamXDSInternal) {
  Network::Address::InstanceConstSharedPtr upstream_address =
      std::make_shared<Network::Address::EnvoyInternalInstance>("internal_address", "endpoint_id");
  std::shared_ptr<NiceMock<Envoy::Upstream::MockHostDescription>> upstream_host(
      new NiceMock<Envoy::Upstream::MockHostDescription>());
  EXPECT_CALL(*upstream_host, address()).WillRepeatedly(Return(upstream_address));
  stream_info_.upstreamInfo()->setUpstreamHost(upstream_host);
  auto host_metadata = std::make_shared<envoy::config::core::v3::Metadata>();
  ON_CALL(*upstream_host, metadata()).WillByDefault(testing::Return(host_metadata));
  TestUtility::loadFromYaml(R"EOF(
  filter_metadata:
    envoy.filters.listener.original_dst:
      local: 127.0.0.100:80
  )EOF",
                            *host_metadata);

  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "foo", "foo", "foo-service",
                                   "v1alpha3", "", "", Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "127.0.0.100")) {
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    upstream_discovery:
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "foo");
}

TEST_F(PeerMetadataTest, DownstreamMXEmpty) {
  initialize(R"EOF(
    downstream_discovery:
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

constexpr absl::string_view SampleIstioHeader =
    "ChIKBWlzdGlvEgkaB3NpZGVjYXIKDgoIU1RTX1BPUlQSAhoAChEKB01FU0hfSUQSBhoEbWVzaAocChZTVEFDS0RSSVZFUl"
    "9UT0tFTl9GSUxFEgIaAAowCihTVEFDS0RSSVZFUl9MT0dHSU5HX0VYUE9SVF9JTlRFUlZBTF9TRUNTEgQaAjIwCjYKDElO"
    "U1RBTkNFX0lQUxImGiQxMC41Mi4wLjM0LGZlODA6OmEwNzU6MTFmZjpmZTVlOmYxY2QKFAoDYXBwEg0aC3Byb2R1Y3RwYW"
    "dlCisKG1NFQ1VSRV9TVEFDS0RSSVZFUl9FTkRQT0lOVBIMGgpsb2NhbGhvc3Q6Cl0KGmt1YmVybmV0ZXMuaW8vbGltaXQt"
    "cmFuZ2VyEj8aPUxpbWl0UmFuZ2VyIHBsdWdpbiBzZXQ6IGNwdSByZXF1ZXN0IGZvciBjb250YWluZXIgcHJvZHVjdHBhZ2"
    "UKIQoNV09SS0xPQURfTkFNRRIQGg5wcm9kdWN0cGFnZS12MQofChFJTlRFUkNFUFRJT05fTU9ERRIKGghSRURJUkVDVAoe"
    "CgpDTFVTVEVSX0lEEhAaDmNsaWVudC1jbHVzdGVyCkkKD0lTVElPX1BST1hZX1NIQRI2GjRpc3Rpby1wcm94eTo0N2U0NT"
    "U5YjhlNGYwZDUxNmMwZDE3YjIzM2QxMjdhM2RlYjNkN2NlClIKBU9XTkVSEkkaR2t1YmVybmV0ZXM6Ly9hcGlzL2FwcHMv"
    "djEvbmFtZXNwYWNlcy9kZWZhdWx0L2RlcGxveW1lbnRzL3Byb2R1Y3RwYWdlLXYxCsEBCgZMQUJFTFMStgEqswEKFAoDYX"
    "BwEg0aC3Byb2R1Y3RwYWdlCiEKEXBvZC10ZW1wbGF0ZS1oYXNoEgwaCjg0OTc1YmM3NzgKMwofc2VydmljZS5pc3Rpby5p"
    "by9jYW5vbmljYWwtbmFtZRIQGg5wcm9kdWN0cGFnZS12MQoyCiNzZXJ2aWNlLmlzdGlvLmlvL2Nhbm9uaWNhbC1yZXZpc2"
    "lvbhILGgl2ZXJzaW9uLTEKDwoHdmVyc2lvbhIEGgJ2MQopCgROQU1FEiEaH3Byb2R1Y3RwYWdlLXYxLTg0OTc1YmM3Nzgt"
    "cHh6MncKLQoIUE9EX05BTUUSIRofcHJvZHVjdHBhZ2UtdjEtODQ5NzViYzc3OC1weHoydwoaCg1JU1RJT19WRVJTSU9OEg"
    "kaBzEuNS1kZXYKHwoVSU5DTFVERV9JTkJPVU5EX1BPUlRTEgYaBDkwODAKmwEKEVBMQVRGT1JNX01FVEFEQVRBEoUBKoIB"
    "CiYKFGdjcF9na2VfY2x1c3Rlcl9uYW1lEg4aDHRlc3QtY2x1c3RlcgocCgxnY3BfbG9jYXRpb24SDBoKdXMtZWFzdDQtYg"
    "odCgtnY3BfcHJvamVjdBIOGgx0ZXN0LXByb2plY3QKGwoSZ2NwX3Byb2plY3RfbnVtYmVyEgUaAzEyMwopCg9TRVJWSUNF"
    "X0FDQ09VTlQSFhoUYm9va2luZm8tcHJvZHVjdHBhZ2UKHQoQQ09ORklHX05BTUVTUEFDRRIJGgdkZWZhdWx0Cg8KB3Zlcn"
    "Npb24SBBoCdjEKHgoYU1RBQ0tEUklWRVJfUk9PVF9DQV9GSUxFEgIaAAohChFwb2QtdGVtcGxhdGUtaGFzaBIMGgo4NDk3"
    "NWJjNzc4Ch8KDkFQUF9DT05UQUlORVJTEg0aC3Rlc3QsYm9uemFpChYKCU5BTUVTUEFDRRIJGgdkZWZhdWx0CjMKK1NUQU"
    "NLRFJJVkVSX01PTklUT1JJTkdfRVhQT1JUX0lOVEVSVkFMX1NFQ1MSBBoCMjA";

TEST_F(PeerMetadataTest, DownstreamFallbackFirst) {
  request_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  EXPECT_CALL(*metadata_provider_, GetMetadata(_)).Times(0);
  initialize(R"EOF(
    downstream_discovery:
      - istio_headers: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "default");
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamFallbackSecond) {
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                                   "v1alpha3", "", "", Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "127.0.0.1")) { // remote address
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    downstream_discovery:
      - istio_headers: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "default");
  checkNoPeer(false);
}

TEST(MXMethod, Cache) {
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  absl::flat_hash_set<std::string> additional_labels;
  MXMethod method(true, additional_labels, context);
  NiceMock<StreamInfo::MockStreamInfo> stream_info;
  Http::TestRequestHeaderMapImpl request_headers;
  const int32_t max = 1000;
  for (int32_t run = 0; run < 3; run++) {
    for (int32_t i = 0; i < max; i++) {
      std::string id = absl::StrCat("test-", i);
      request_headers.setReference(Headers::get().ExchangeMetadataHeaderId, id);
      request_headers.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
      Context ctx;
      const auto result = method.derivePeerInfo(stream_info, request_headers, ctx);
      EXPECT_TRUE(result.has_value());
    }
  }
}

TEST_F(PeerMetadataTest, DownstreamMX) {
  request_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    downstream_discovery:
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "default");
  checkNoPeer(false);
  checkShared(false);
}

TEST_F(PeerMetadataTest, UpstreamMX) {
  response_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  response_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    upstream_discovery:
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "default");
}

TEST_F(PeerMetadataTest, UpstreamFallbackFirst) {
  EXPECT_CALL(*metadata_provider_, GetMetadata(_)).Times(0);
  response_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  response_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    upstream_discovery:
      - istio_headers: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "default");
}

TEST_F(PeerMetadataTest, UpstreamFallbackSecond) {
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "foo", "foo", "foo-service",
                                   "v1alpha3", "", "", Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "10.0.0.1")) { // upstream host address
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    upstream_discovery:
      - istio_headers: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "foo");
}

TEST_F(PeerMetadataTest, UpstreamFallbackFirstXDS) {
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "foo", "foo", "foo-service",
                                   "v1alpha3", "", "", Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "10.0.0.1")) { // upstream host address
          return {pod};
        }
        return {};
      }));
  response_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  response_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    upstream_discovery:
      - workload_discovery: {}
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "foo");
}

TEST_F(PeerMetadataTest, DownstreamMXPropagation) {
  initialize(R"EOF(
    downstream_propagation:
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamMXPropagationWithAdditionalLabels) {
  initialize(R"EOF(
    downstream_propagation:
      - istio_headers: {}
    additional_labels:
      - foo
      - bar
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamMXDiscoveryPropagation) {
  request_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    downstream_discovery:
      - istio_headers: {}
    downstream_propagation:
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(2, response_headers_.size());
  checkPeerNamespace(true, "default");
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamMXPropagation) {
  initialize(R"EOF(
    upstream_propagation:
      - istio_headers:
          skip_external_clusters: false
  )EOF");
  EXPECT_EQ(2, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamMXPropagationSkipNoMatch) {
  initialize(R"EOF(
    upstream_propagation:
      - istio_headers:
          skip_external_clusters: true
  )EOF");
  EXPECT_EQ(2, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamMXPropagationSkip) {
  std::shared_ptr<Upstream::MockClusterInfo> cluster_info_{
      std::make_shared<NiceMock<Upstream::MockClusterInfo>>()};
  auto metadata = TestUtility::parseYaml<envoy::config::core::v3::Metadata>(R"EOF(
      filter_metadata:
        istio:
          external: true
    )EOF");
  ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(testing::Return(cluster_info_));
  ON_CALL(*cluster_info_, metadata()).WillByDefault(ReturnRef(metadata));
  initialize(R"EOF(
    upstream_propagation:
      - istio_headers:
          skip_external_clusters: true
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamMXPropagationSkipPassthrough) {
  std::shared_ptr<Upstream::MockClusterInfo> cluster_info_{
      std::make_shared<NiceMock<Upstream::MockClusterInfo>>()};
  cluster_info_->name_ = "PassthroughCluster";
  ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(testing::Return(cluster_info_));
  initialize(R"EOF(
    upstream_propagation:
      - istio_headers:
          skip_external_clusters: true
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamBaggagePropagation) {
  initialize(R"EOF(
    downstream_propagation:
      - baggage: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(1, response_headers_.size());
  EXPECT_TRUE(response_headers_.has(Headers::get().Baggage));
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamBaggagePropagation) {
  initialize(R"EOF(
    upstream_propagation:
      - baggage: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  EXPECT_TRUE(request_headers_.has(Headers::get().Baggage));
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, BothDirectionsBaggagePropagation) {
  initialize(R"EOF(
    downstream_propagation:
      - baggage: {}
    upstream_propagation:
      - baggage: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(1, response_headers_.size());
  EXPECT_TRUE(request_headers_.has(Headers::get().Baggage));
  EXPECT_TRUE(response_headers_.has(Headers::get().Baggage));
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, BaggagePropagationWithNodeMetadata) {
  // Setup node metadata that would be converted to baggage
  auto& node = context_.server_factory_context_.local_info_.node_;
  TestUtility::loadFromYaml(R"EOF(
    metadata:
      NAMESPACE: production
      CLUSTER_ID: test-cluster
      WORKLOAD_NAME: test-workload
      NAME: test-instance
      LABELS:
        app: test-app
        version: v1.0
        service.istio.io/canonical-name: test-service
        service.istio.io/canonical-revision: main
  )EOF",
                            node);

  initialize(R"EOF(
    downstream_propagation:
      - baggage: {}
  )EOF");

  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(1, response_headers_.size());

  const auto baggage_header = response_headers_.get(Headers::get().Baggage);
  ASSERT_FALSE(baggage_header.empty());

  std::string baggage_value = std::string(baggage_header[0]->value().getStringView());
  // Verify baggage contains expected key-value pairs
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.namespace.name=production"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.cluster.name=test-cluster"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "app.name=test-app"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "app.version=v1.0"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "service.name=test-service"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "service.version=main"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.workload.name=test-workload"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.instance.name=test-instance"));
}

// Test class specifically for BaggagePropagationMethod unit tests
class BaggagePropagationMethodTest : public testing::Test {
protected:
  BaggagePropagationMethodTest() = default;

  void SetUp() override {
    TestUtility::loadFromYaml(R"EOF(
      metadata:
        NAMESPACE: test-namespace
        CLUSTER_ID: sample-cluster
        WORKLOAD_NAME: sample-workload
        NAME: sample-instance
        LABELS:
          app: sample-app
          version: v2.1
          service.istio.io/canonical-name: sample-service
          service.istio.io/canonical-revision: stable
    )EOF",
                              context_.server_factory_context_.local_info_.node_);
  }

  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
};

TEST_F(BaggagePropagationMethodTest, DownstreamBaggageInjection) {
  io::istio::http::peer_metadata::Config_Baggage baggage_config;
  BaggagePropagationMethod method(context_.server_factory_context_, baggage_config);

  Http::TestResponseHeaderMapImpl headers;
  Context ctx;

  method.inject(stream_info_, headers, ctx);

  EXPECT_EQ(1, headers.size());
  const auto baggage_header = headers.get(Headers::get().Baggage);
  ASSERT_FALSE(baggage_header.empty());

  std::string baggage_value = std::string(baggage_header[0]->value().getStringView());

  // Verify all expected tokens are present
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.namespace.name=test-namespace"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.cluster.name=sample-cluster"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "service.name=sample-service"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "service.version=stable"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "app.name=sample-app"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "app.version=v2.1"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.workload.name=sample-workload"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.instance.name=sample-instance"));
}

TEST_F(BaggagePropagationMethodTest, UpstreamBaggageInjection) {
  io::istio::http::peer_metadata::Config_Baggage baggage_config;
  BaggagePropagationMethod method(context_.server_factory_context_, baggage_config);

  Http::TestRequestHeaderMapImpl headers;
  Context ctx;

  method.inject(stream_info_, headers, ctx);

  EXPECT_EQ(1, headers.size());
  const auto baggage_header = headers.get(Headers::get().Baggage);
  ASSERT_FALSE(baggage_header.empty());

  std::string baggage_value = std::string(baggage_header[0]->value().getStringView());

  // Verify tokens are properly formatted
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.namespace.name=test-namespace"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.cluster.name=sample-cluster"));

  // Check that values are comma-separated
  std::vector<absl::string_view> parts = absl::StrSplit(baggage_value, ',');
  EXPECT_GT(parts.size(), 1);

  // Each part should be in key=value format
  for (const auto& part : parts) {
    EXPECT_TRUE(absl::StrContains(part, "="));
  }
}

TEST_F(BaggagePropagationMethodTest, EmptyMetadataBaggage) {
  // Reset node metadata to empty
  context_.server_factory_context_.local_info_.node_.Clear();

  io::istio::http::peer_metadata::Config_Baggage baggage_config;
  BaggagePropagationMethod method(context_.server_factory_context_, baggage_config);

  Http::TestResponseHeaderMapImpl headers;
  Context ctx;

  method.inject(stream_info_, headers, ctx);

  EXPECT_EQ(1, headers.size());
  const auto baggage_header = headers.get(Headers::get().Baggage);
  ASSERT_FALSE(baggage_header.empty());

  // With empty metadata, baggage should contain only default workload type
  std::string baggage_value = std::string(baggage_header[0]->value().getStringView());
  EXPECT_EQ("k8s.workload.type=unknown", baggage_value);
}

TEST_F(BaggagePropagationMethodTest, PartialMetadataBaggage) {
  // Setup node metadata with only some fields
  TestUtility::loadFromYaml(R"EOF(
    metadata:
      NAMESPACE: partial-namespace
      LABELS:
        app: partial-app
        # Missing other fields like version, cluster, etc.
  )EOF",
                            context_.server_factory_context_.local_info_.node_);

  io::istio::http::peer_metadata::Config_Baggage baggage_config;
  BaggagePropagationMethod method(context_.server_factory_context_, baggage_config);

  Http::TestRequestHeaderMapImpl headers;
  Context ctx;

  method.inject(stream_info_, headers, ctx);

  EXPECT_EQ(1, headers.size());
  const auto baggage_header = headers.get(Headers::get().Baggage);
  ASSERT_FALSE(baggage_header.empty());

  std::string baggage_value = std::string(baggage_header[0]->value().getStringView());

  // Should contain only the fields that were present
  EXPECT_TRUE(absl::StrContains(baggage_value, "k8s.namespace.name=partial-namespace"));
  EXPECT_TRUE(absl::StrContains(baggage_value, "app.name=partial-app"));

  // Should not contain fields that were not present
  EXPECT_FALSE(absl::StrContains(baggage_value, "app.version="));
  EXPECT_FALSE(absl::StrContains(baggage_value, "k8s.cluster.name="));
}

TEST_F(PeerMetadataTest, BaggagePropagationWithMixedConfig) {
  initialize(R"EOF(
    downstream_propagation:
      - baggage: {}
      - istio_headers: {}
    upstream_propagation:
      - baggage: {}
      - istio_headers: {}
  )EOF");

  // Baggage should always be propagated, Istio headers are also propagated for upstream only
  EXPECT_EQ(3, request_headers_.size());  // baggage + istio headers (id + metadata)
  EXPECT_EQ(1, response_headers_.size()); // baggage only (no discovery, so no MX downstream)

  EXPECT_TRUE(request_headers_.has(Headers::get().Baggage));
  EXPECT_TRUE(request_headers_.has(Headers::get().ExchangeMetadataHeaderId));
  EXPECT_TRUE(request_headers_.has(Headers::get().ExchangeMetadataHeader));

  EXPECT_TRUE(response_headers_.has(Headers::get().Baggage));
}

// Baggage Discovery Tests

TEST_F(PeerMetadataTest, DownstreamBaggageDiscoveryEmpty) {
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamBaggageDiscoveryEmpty) {
  initialize(R"EOF(
    upstream_discovery:
      - baggage: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamBaggageDiscovery) {
  request_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=test-namespace,k8s.cluster.name=test-cluster,"
      "service.name=test-service,service.version=v1,k8s.deployment.name=test-workload,"
      "k8s.workload.type=deployment,k8s.instance.name=test-instance-123,"
      "app.name=test-app,app.version=v2.0");
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "test-namespace");
  checkNoPeer(false);
  checkShared(false);
}

TEST_F(PeerMetadataTest, UpstreamBaggageDiscovery) {
  response_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=upstream-namespace,k8s.cluster.name=upstream-cluster,"
      "service.name=upstream-service,service.version=v2,k8s.workload.name=upstream-workload,"
      "k8s.workload.type=pod,k8s.instance.name=upstream-instance-456,"
      "app.name=upstream-app,app.version=v3.0");
  initialize(R"EOF(
    upstream_discovery:
      - baggage: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(1, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "upstream-namespace");
}

TEST_F(PeerMetadataTest, BothDirectionsBaggageDiscovery) {
  request_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=downstream-ns,service.name=downstream-svc");
  response_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=upstream-ns,service.name=upstream-svc");
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
    upstream_discovery:
      - baggage: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(1, response_headers_.size());
  checkPeerNamespace(true, "downstream-ns");
  checkPeerNamespace(false, "upstream-ns");
}

TEST_F(PeerMetadataTest, DownstreamBaggageFallbackFirst) {
  // Baggage is present, so XDS should not be called
  request_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=baggage-namespace,service.name=baggage-service");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_)).Times(0);
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "baggage-namespace");
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamBaggageFallbackSecond) {
  // No baggage header, so XDS should be called as fallback
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "xds-namespace", "foo",
                                   "foo-service", "v1alpha3", "", "",
                                   Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "127.0.0.1")) {
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "xds-namespace");
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, UpstreamBaggageFallbackFirst) {
  // Baggage is present, so XDS should not be called
  response_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=baggage-upstream,service.name=baggage-upstream-service");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_)).Times(0);
  initialize(R"EOF(
    upstream_discovery:
      - baggage: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(1, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "baggage-upstream");
}

TEST_F(PeerMetadataTest, UpstreamBaggageFallbackSecond) {
  // No baggage header, so XDS should be called as fallback
  const WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "xds-upstream", "foo",
                                   "foo-service", "v1alpha3", "", "",
                                   Istio::Common::WorkloadType::Pod, "");
  EXPECT_CALL(*metadata_provider_, GetMetadata(_))
      .WillRepeatedly(Invoke([&](const Network::Address::InstanceConstSharedPtr& address)
                                 -> std::optional<WorkloadMetadataObject> {
        if (absl::StartsWith(address->asStringView(), "10.0.0.1")) {
          return {pod};
        }
        return {};
      }));
  initialize(R"EOF(
    upstream_discovery:
      - baggage: {}
      - workload_discovery: {}
  )EOF");
  EXPECT_EQ(0, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkNoPeer(true);
  checkPeerNamespace(false, "xds-upstream");
}

TEST_F(PeerMetadataTest, DownstreamBaggageWithMXFallback) {
  // Baggage is present, so MX should not be used
  request_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=baggage-ns,service.name=baggage-svc");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
      - istio_headers: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  checkPeerNamespace(true, "baggage-ns");
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, DownstreamMXWithBaggageFallback) {
  // MX is first, so it should be used even if baggage is present
  request_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=baggage-ns,service.name=baggage-svc");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeaderId, "test-pod");
  request_headers_.setReference(Headers::get().ExchangeMetadataHeader, SampleIstioHeader);
  initialize(R"EOF(
    downstream_discovery:
      - istio_headers: {}
      - baggage: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());
  EXPECT_EQ(0, response_headers_.size());
  // MX header has namespace "default" from SampleIstioHeader
  checkPeerNamespace(true, "default");
  checkNoPeer(false);
}

TEST_F(PeerMetadataTest, BaggageDiscoveryWithPropagation) {
  request_headers_.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=discovered-ns,service.name=discovered-svc");
  initialize(R"EOF(
    downstream_discovery:
      - baggage: {}
    downstream_propagation:
      - baggage: {}
    upstream_propagation:
      - baggage: {}
  )EOF");
  EXPECT_EQ(1, request_headers_.size());  // upstream baggage propagation
  EXPECT_EQ(1, response_headers_.size()); // downstream baggage propagation
  EXPECT_TRUE(request_headers_.has(Headers::get().Baggage));
  EXPECT_TRUE(response_headers_.has(Headers::get().Baggage));
  checkPeerNamespace(true, "discovered-ns");
  checkNoPeer(false);
}

// Test class specifically for BaggageDiscoveryMethod unit tests
class BaggageDiscoveryMethodTest : public testing::Test {
protected:
  BaggageDiscoveryMethodTest() = default;

  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
};

TEST_F(BaggageDiscoveryMethodTest, DerivePeerInfoFromBaggage) {
  BaggageDiscoveryMethod method(true, context_.server_factory_context_);

  Http::TestRequestHeaderMapImpl headers;
  headers.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=unit-test-namespace,k8s.cluster.name=unit-test-cluster,"
      "service.name=unit-test-service,service.version=v1.0,"
      "k8s.deployment.name=unit-test-workload,k8s.workload.type=deployment,"
      "k8s.instance.name=unit-test-instance,app.name=unit-test-app,app.version=v2.0");
  Context ctx;

  const auto result = method.derivePeerInfo(stream_info_, headers, ctx);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("unit-test-namespace", result->namespace_name_);
  EXPECT_EQ("unit-test-cluster", result->cluster_name_);
  EXPECT_EQ("unit-test-service", result->canonical_name_);
  EXPECT_EQ("v1.0", result->canonical_revision_);
  EXPECT_EQ("unit-test-workload", result->workload_name_);
  EXPECT_EQ("unit-test-instance", result->instance_name_);
  EXPECT_EQ("unit-test-app", result->app_name_);
  EXPECT_EQ("v2.0", result->app_version_);
  EXPECT_EQ(Istio::Common::WorkloadType::Deployment, result->workload_type_);
}

TEST_F(BaggageDiscoveryMethodTest, DerivePeerInfoEmptyBaggage) {
  BaggageDiscoveryMethod method(true, context_.server_factory_context_);

  Http::TestRequestHeaderMapImpl headers;
  Context ctx;

  const auto result = method.derivePeerInfo(stream_info_, headers, ctx);

  EXPECT_FALSE(result.has_value());
}

TEST_F(BaggageDiscoveryMethodTest, DerivePeerInfoPartialBaggage) {
  BaggageDiscoveryMethod method(false, context_.server_factory_context_);

  Http::TestResponseHeaderMapImpl headers;
  headers.setReference(
      Headers::get().Baggage,
      "k8s.namespace.name=partial-ns,service.name=partial-svc");
  Context ctx;

  const auto result = method.derivePeerInfo(stream_info_, headers, ctx);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("partial-ns", result->namespace_name_);
  EXPECT_EQ("partial-svc", result->canonical_name_);
  // Other fields should be empty or default
  EXPECT_TRUE(result->cluster_name_.empty());
  EXPECT_TRUE(result->workload_name_.empty());
}

TEST_F(BaggageDiscoveryMethodTest, DerivePeerInfoAllWorkloadTypes) {
  BaggageDiscoveryMethod method(true, context_.server_factory_context_);
  Context ctx;

  // Test Pod workload type
  {
    Http::TestRequestHeaderMapImpl headers;
    headers.setReference(Headers::get().Baggage,
                         "k8s.namespace.name=test-ns,k8s.pod.name=pod-name");
    const auto result = method.derivePeerInfo(stream_info_, headers, ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(Istio::Common::WorkloadType::Pod, result->workload_type_);
  }

  // Test Deployment workload type
  {
    Http::TestRequestHeaderMapImpl headers;
    headers.setReference(Headers::get().Baggage,
                         "k8s.namespace.name=test-ns,k8s.deployment.name=deployment-name");
    const auto result = method.derivePeerInfo(stream_info_, headers, ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(Istio::Common::WorkloadType::Deployment, result->workload_type_);
  }

  // Test Job workload type
  {
    Http::TestRequestHeaderMapImpl headers;
    headers.setReference(Headers::get().Baggage,
                         "k8s.namespace.name=test-ns,k8s.job.name=job-name");
    const auto result = method.derivePeerInfo(stream_info_, headers, ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(Istio::Common::WorkloadType::Job, result->workload_type_);
  }

  // Test CronJob workload type
  {
    Http::TestRequestHeaderMapImpl headers;
    headers.setReference(Headers::get().Baggage,
                         "k8s.namespace.name=test-ns,k8s.cronjob.name=cronjob-name");
    const auto result = method.derivePeerInfo(stream_info_, headers, ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(Istio::Common::WorkloadType::CronJob, result->workload_type_);
  }
}

} // namespace
} // namespace PeerMetadata
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
