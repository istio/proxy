#include "source/extensions/filters/network/peer_metadata/peer_metadata.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"

#include "envoy/router/string_accessor.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "source/common/http/utility.h"
#include "source/common/network/address_impl.h"
#include "source/common/tcp_proxy/tcp_proxy.h"

#include "test/mocks/local_info/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/upstream/host.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PeerMetadata {
namespace {

constexpr std::string_view defaultBaggageKey = "io.istio.baggage";
constexpr std::string_view defaultIdentity = "spiffe://cluster.local/ns/default/sa/default";
constexpr std::string_view defaultBaggage =
    "k8s.deployment.name=server,service.name=server-service,service.version=v2,k8s.namespace.name="
    "default,k8s.cluster.name=cluster";

using ::testing::Const;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

bool parsePeerMetadataHeader(const std::string& data, PeerMetadataHeader& header) {
  if (data.size() < sizeof(header)) {
    return false;
  }
  std::memcpy(&header, data.data(), sizeof(header));
  return true;
}

bool parsePeerMetadata(const std::string& data, PeerMetadataHeader& header,
                       google::protobuf::Any& any) {
  if (!parsePeerMetadataHeader(data, header)) {
    return false;
  }
  return any.ParseFromArray(data.data() + sizeof(header), data.size() - sizeof(header));
}

Config configWithBaggageKey(std::string_view baggage_key) {
  Config config;
  config.set_baggage_key(baggage_key);
  return config;
}

class PeerMetadataFilterTest : public ::testing::Test {
public:
  PeerMetadataFilterTest() {}

  void populateNodeMetadata(std::string_view workload_name, std::string_view workload_type,
                            std::string_view service_name, std::string_view service_version,
                            std::string_view ns, std::string_view cluster) {
    auto metadata = node_metadata_.mutable_metadata()->mutable_fields();
    (*metadata)["NAMESPACE"].set_string_value(ns);
    (*metadata)["CLUSTER_ID"].set_string_value(cluster);
    (*metadata)["WORKLOAD_NAME"].set_string_value(workload_name);
    (*metadata)["OWNER"].set_string_value(absl::StrCat("kubernetes://apis/apps/v1/namespaces/", ns,
                                                       "/", workload_type, "s/", workload_name));
    auto labels = (*metadata)["LABELS"].mutable_struct_value()->mutable_fields();
    (*labels)["service.istio.io/canonical-name"].set_string_value(service_name);
    (*labels)["service.istio.io/canonical-revision"].set_string_value(service_version);
  }

  void populateTcpProxyResponseHeaders(const Http::ResponseHeaderMap& response_headers) {
    auto headers = std::make_shared<TcpProxy::TunnelResponseHeaders>(
        std::make_unique<Http::TestResponseHeaderMapImpl>(response_headers));
    auto filter_state = stream_info_.filterState();
    filter_state->setData(TcpProxy::TunnelResponseHeaders::key(), headers,
                          StreamInfo::FilterState::StateType::Mutable,
                          StreamInfo::FilterState::LifeSpan::Connection);
  }

  void populateUpstreamSans(const std::vector<std::string>& sans) {
    sans_ = sans;
    auto ssl = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
    ON_CALL(*ssl, uriSanPeerCertificate()).WillByDefault(Return(sans_));
    auto upstream = stream_info_.upstreamInfo();
    upstream->setUpstreamSslConnection(ssl);
  }

  void disablePeerDiscovery() {
    google::protobuf::Struct metadata;
    auto fields = metadata.mutable_fields();
    (*fields)[FilterNames::get().DisableDiscoveryField].set_bool_value(true);
    (*stream_info_.metadata_.mutable_filter_metadata())[FilterNames::get().Name].MergeFrom(
        metadata);
  }

  void initialize(const Config& config) {
    ON_CALL(local_info_, node()).WillByDefault(ReturnRef(node_metadata_));

    ON_CALL(read_filter_callbacks_.connection_, streamInfo())
        .WillByDefault(ReturnRef(stream_info_));
    ON_CALL(Const(read_filter_callbacks_.connection_), streamInfo())
        .WillByDefault(ReturnRef(stream_info_));
    ON_CALL(write_filter_callbacks_.connection_, streamInfo())
        .WillByDefault(ReturnRef(stream_info_));
    ON_CALL(Const(write_filter_callbacks_.connection_), streamInfo())
        .WillByDefault(ReturnRef(stream_info_));

    filter_ = std::make_unique<Filter>(config, local_info_);
    filter_->initializeReadFilterCallbacks(read_filter_callbacks_);
    filter_->initializeWriteFilterCallbacks(write_filter_callbacks_);
  }

  ::envoy::config::core::v3::Node node_metadata_;
  NiceMock<LocalInfo::MockLocalInfo> local_info_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Network::MockReadFilterCallbacks> read_filter_callbacks_;
  NiceMock<Network::MockWriteFilterCallbacks> write_filter_callbacks_;
  std::vector<std::string> sans_;
  std::unique_ptr<Filter> filter_;
};

TEST_F(PeerMetadataFilterTest, TestPopulateBaggage) {
  populateNodeMetadata("workload", "deployment", "workload-service", "v1", "default", "cluster");
  initialize(configWithBaggageKey(defaultBaggageKey));
  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  const auto filter_state = stream_info_.filterState();
  EXPECT_TRUE(filter_state->hasDataWithName(defaultBaggageKey));
  const auto* baggage = filter_state->getDataReadOnly<Router::StringAccessor>(defaultBaggageKey);
  EXPECT_EQ(baggage->asString(),
            "k8s.deployment.name=workload,service.name=workload-service,service.version=v1,k8s."
            "namespace.name=default,k8s.cluster.name=cluster");
}

TEST_F(PeerMetadataFilterTest, TestDoNotPopulateBaggage) {
  populateNodeMetadata("workload", "deployment", "workload-service", "v1", "default", "cluster");
  initialize(Config{});
  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  const auto filter_state = stream_info_.filterState();
  EXPECT_FALSE(
      filter_state->hasDataAtOrAboveLifeSpan(StreamInfo::FilterState::LifeSpan::FilterChain));
}

TEST_F(PeerMetadataFilterTest, TestPeerMetadataDiscoveredAndPropagated) {
  const std::string baggage{defaultBaggage};
  const std::string identity{defaultIdentity};

  populateNodeMetadata("workload", "deployment", "workload-service", "v1", "default", "cluster");
  initialize(configWithBaggageKey(defaultBaggageKey));

  Http::TestResponseHeaderMapImpl headers{{Headers::get().Baggage.get(), baggage}};
  populateTcpProxyResponseHeaders(headers);

  std::vector<std::string> sans{identity};
  populateUpstreamSans(sans);

  std::string injected;
  EXPECT_CALL(write_filter_callbacks_, injectWriteDataToFilterChain(_, false))
      .WillRepeatedly(Invoke([&injected](Buffer::Instance& buffer, bool) {
        injected = std::string(static_cast<const char*>(buffer.linearize(buffer.length())),
                               buffer.length());
      }));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  Buffer::OwnedImpl data;
  EXPECT_EQ(filter_->onWrite(data, /*end_stream*/ false), Network::FilterStatus::Continue);
  EXPECT_FALSE(injected.empty());

  PeerMetadataHeader header;
  google::protobuf::Any any;
  EXPECT_TRUE(parsePeerMetadata(injected, header, any));
  EXPECT_EQ(header.magic, PeerMetadataHeader::magic_number);
  EXPECT_GT(header.data_size, 0);

  google::protobuf::Struct metadata;
  EXPECT_TRUE(any.UnpackTo(&metadata));

  std::unique_ptr<Istio::Common::WorkloadMetadataObject> workload =
      Istio::Common::convertStructToWorkloadMetadata(metadata);
  EXPECT_EQ(workload->baggage(), baggage);
  EXPECT_EQ(workload->identity(), identity);
}

TEST_F(PeerMetadataFilterTest, TestNoFiltersStateFromTcpProxy) {
  const std::string identity{defaultIdentity};

  populateNodeMetadata("workload", "deployment", "workload-service", "v1", "default", "cluster");
  initialize(configWithBaggageKey(defaultBaggageKey));

  std::vector<std::string> sans{identity};
  populateUpstreamSans(sans);

  std::string injected;
  EXPECT_CALL(write_filter_callbacks_, injectWriteDataToFilterChain(_, false))
      .WillRepeatedly(Invoke([&injected](Buffer::Instance& buffer, bool) {
        injected = std::string(static_cast<const char*>(buffer.linearize(buffer.length())),
                               buffer.length());
      }));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  Buffer::OwnedImpl data;
  EXPECT_EQ(filter_->onWrite(data, /*end_stream*/ false), Network::FilterStatus::Continue);
  EXPECT_FALSE(injected.empty());

  PeerMetadataHeader header;
  EXPECT_TRUE(parsePeerMetadataHeader(injected, header));
  EXPECT_EQ(header.magic, PeerMetadataHeader::magic_number);
  EXPECT_EQ(header.data_size, 0);
}

TEST_F(PeerMetadataFilterTest, TestNoBaggageHeaderFromTcpProxy) {
  const std::string baggage{defaultBaggage};
  const std::string identity{defaultIdentity};

  populateNodeMetadata("workload", "deployment", "workload-service", "v1", "default", "cluster");
  initialize(configWithBaggageKey(defaultBaggageKey));

  Http::TestResponseHeaderMapImpl headers{{"bibbage", baggage}};
  populateTcpProxyResponseHeaders(headers);

  std::vector<std::string> sans{identity};
  populateUpstreamSans(sans);

  std::string injected;
  EXPECT_CALL(write_filter_callbacks_, injectWriteDataToFilterChain(_, false))
      .WillRepeatedly(Invoke([&injected](Buffer::Instance& buffer, bool) {
        injected = std::string(static_cast<const char*>(buffer.linearize(buffer.length())),
                               buffer.length());
      }));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  Buffer::OwnedImpl data;
  EXPECT_EQ(filter_->onWrite(data, /*end_stream*/ false), Network::FilterStatus::Continue);
  EXPECT_FALSE(injected.empty());

  PeerMetadataHeader header;
  EXPECT_TRUE(parsePeerMetadataHeader(injected, header));
  EXPECT_EQ(header.magic, PeerMetadataHeader::magic_number);
  EXPECT_EQ(header.data_size, 0);
}

TEST_F(PeerMetadataFilterTest, TestDisablePeerDiscovery) {
  const std::string baggage{defaultBaggage};
  const std::string identity{defaultIdentity};

  populateNodeMetadata("workload", "deployment", "workload-service", "v1", "default", "cluster");
  initialize(configWithBaggageKey(defaultBaggageKey));

  Http::TestResponseHeaderMapImpl headers{{Headers::get().Baggage.get(), baggage}};
  populateTcpProxyResponseHeaders(headers);

  std::vector<std::string> sans{identity};
  populateUpstreamSans(sans);

  disablePeerDiscovery();

  std::string injected;
  ON_CALL(write_filter_callbacks_, injectWriteDataToFilterChain(_, false))
      .WillByDefault(Invoke([&injected](Buffer::Instance& buffer, bool) {
        injected = std::string(static_cast<const char*>(buffer.linearize(buffer.length())),
                               buffer.length());
      }));
  EXPECT_CALL(write_filter_callbacks_, injectWriteDataToFilterChain(_, false)).Times(0);

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  Buffer::OwnedImpl data;
  EXPECT_EQ(filter_->onWrite(data, /*end_stream*/ false), Network::FilterStatus::Continue);
  EXPECT_TRUE(injected.empty());
}

std::shared_ptr<NiceMock<Upstream::MockHostDescription>>
makeInternalListenerHost(std::string_view listener_name) {
  auto host = std::make_shared<NiceMock<Upstream::MockHostDescription>>();
  host->address_ =
      std::make_shared<Network::Address::EnvoyInternalInstance>(std::string(listener_name));
  ON_CALL(*host, address()).WillByDefault(Return(host->address_));
  return host;
}

std::shared_ptr<NiceMock<Upstream::MockHostDescription>> makeIpv4Host(std::string_view address) {
  auto host = std::make_shared<NiceMock<Upstream::MockHostDescription>>();
  host->address_ = std::make_shared<Network::Address::Ipv4Instance>(std::string(address));
  ON_CALL(*host, address()).WillByDefault(Return(host->address_));
  return host;
}

std::string encodePeerMetadataHeaderOnly(const PeerMetadataHeader& header) {
  return std::string(std::string_view(reinterpret_cast<const char*>(&header), sizeof(header)));
}

std::string encodeMetadataOnly(std::string_view baggage, std::string_view identity) {
  std::unique_ptr<::Istio::Common::WorkloadMetadataObject> metadata =
      ::Istio::Common::convertBaggageToWorkloadMetadata(baggage, identity);
  google::protobuf::Struct data = convertWorkloadMetadataToStruct(*metadata);
  google::protobuf::Any wrapped;
  wrapped.PackFrom(data);
  return wrapped.SerializeAsString();
}

std::string encodePeerMetadata(std::string_view baggage, std::string_view identity) {
  std::string metadata = encodeMetadataOnly(baggage, identity);
  PeerMetadataHeader header{PeerMetadataHeader::magic_number,
                            static_cast<uint32_t>(metadata.size())};
  return encodePeerMetadataHeaderOnly(header) + metadata;
}

class PeerMetadataUpstreamFilterTest : public ::testing::Test {
public:
  PeerMetadataUpstreamFilterTest() {}

  void initialize() {
    ON_CALL(callbacks_.connection_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(Const(callbacks_.connection_), streamInfo()).WillByDefault(ReturnRef(stream_info_));

    host_metadata_ = std::make_shared<::envoy::config::core::v3::Metadata>();

    filter_ = std::make_unique<UpstreamFilter>();
    filter_->initializeReadFilterCallbacks(callbacks_);
  }

  ::envoy::config::core::v3::Metadata& hostMetadata() { return *host_metadata_; }

  ::envoy::config::core::v3::Metadata& clusterMetadata() {
    return upstream_host_->cluster_.metadata_;
  }

  std::optional<::Istio::Common::WorkloadMetadataObject> peerInfoFromFilterState() const {
    const auto& filter_state = stream_info_.filterState();
    const auto* cel_state =
        filter_state.getDataReadOnly<Extensions::Filters::Common::Expr::CelState>(
            ::Istio::Common::UpstreamPeer);
    if (!cel_state) {
      return std::nullopt;
    }

    google::protobuf::Struct obj;
    if (!obj.ParseFromString(std::string_view(cel_state->value()))) {
      return std::nullopt;
    }

    std::unique_ptr<::Istio::Common::WorkloadMetadataObject> peer_info =
        ::Istio::Common::convertStructToWorkloadMetadata(obj);
    if (!peer_info) {
      return std::nullopt;
    }

    return *peer_info;
  }

  void setUpstreamHost(const std::shared_ptr<NiceMock<Upstream::MockHostDescription>>& host) {
    upstream_host_ = host;
    ON_CALL(Const(*upstream_host_), metadata()).WillByDefault(Return(host_metadata_));
    stream_info_.upstreamInfo()->setUpstreamHost(upstream_host_);
  }

  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Network::MockReadFilterCallbacks> callbacks_;
  std::shared_ptr<NiceMock<Upstream::MockHostDescription>> upstream_host_;
  std::shared_ptr<::envoy::config::core::v3::Metadata> host_metadata_;
  std::unique_ptr<UpstreamFilter> filter_;
};

TEST_F(PeerMetadataUpstreamFilterTest, TestPeerMetadataConsumedAndPropagated) {
  initialize();
  setUpstreamHost(makeInternalListenerHost("connect_originate"));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  Buffer::OwnedImpl data;
  data.add(encodePeerMetadata(defaultBaggage, defaultIdentity));
  EXPECT_EQ(filter_->onData(data, /*end_stream*/ false), Network::FilterStatus::Continue);

  EXPECT_EQ(data.length(), 0);
  const auto peer_info = peerInfoFromFilterState();
  EXPECT_TRUE(peer_info.has_value());
  EXPECT_EQ(peer_info->identity(), defaultIdentity);
  EXPECT_EQ(peer_info->baggage(), defaultBaggage);
}

TEST_F(PeerMetadataUpstreamFilterTest, TestPeerMetadataNotConsumedForUnknownInternalListeners) {
  initialize();
  setUpstreamHost(makeInternalListenerHost("not_connect_originate"));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  std::string peer_metadata = encodePeerMetadata(defaultBaggage, defaultIdentity);
  Buffer::OwnedImpl data;
  data.add(peer_metadata);
  EXPECT_EQ(filter_->onData(data, /*end_stream*/ false), Network::FilterStatus::Continue);
  EXPECT_EQ(data.length(), peer_metadata.size());
  const auto peer_info = peerInfoFromFilterState();
  EXPECT_FALSE(peer_info.has_value());
}

TEST_F(PeerMetadataUpstreamFilterTest, TestPeerMetadataNotConsumedForExternalHosts) {
  initialize();
  setUpstreamHost(makeIpv4Host("192.168.0.1"));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  std::string peer_metadata = encodePeerMetadata(defaultBaggage, defaultIdentity);
  Buffer::OwnedImpl data;
  data.add(peer_metadata);
  EXPECT_EQ(filter_->onData(data, /*end_stream*/ false), Network::FilterStatus::Continue);
  EXPECT_EQ(data.length(), peer_metadata.size());
  const auto peer_info = peerInfoFromFilterState();
  EXPECT_FALSE(peer_info.has_value());
}

TEST_F(PeerMetadataUpstreamFilterTest, TestPeerMetadataNotConsumedWhenDisabledViaHostMetadata) {
  initialize();
  setUpstreamHost(makeInternalListenerHost("connect_originate"));

  google::protobuf::Struct metadata;
  auto fields = metadata.mutable_fields();
  (*fields)[FilterNames::get().DisableDiscoveryField].set_bool_value(true);
  (*hostMetadata().mutable_filter_metadata())[FilterNames::get().Name].MergeFrom(metadata);

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  std::string peer_metadata = encodePeerMetadata(defaultBaggage, defaultIdentity);
  Buffer::OwnedImpl data;
  data.add(peer_metadata);
  EXPECT_EQ(filter_->onData(data, /*end_stream*/ false), Network::FilterStatus::Continue);

  EXPECT_EQ(data.length(), peer_metadata.size());
  const auto peer_info = peerInfoFromFilterState();
  EXPECT_FALSE(peer_info.has_value());
}

TEST_F(PeerMetadataUpstreamFilterTest, TestPeerMetadataNotConsumedWhenDisabledViaClusterMetadata) {
  initialize();
  setUpstreamHost(makeInternalListenerHost("connect_originate"));

  google::protobuf::Struct metadata;
  auto fields = metadata.mutable_fields();
  (*fields)[FilterNames::get().DisableDiscoveryField].set_bool_value(true);
  (*clusterMetadata().mutable_filter_metadata())[FilterNames::get().Name].MergeFrom(metadata);

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  std::string peer_metadata = encodePeerMetadata(defaultBaggage, defaultIdentity);
  Buffer::OwnedImpl data;
  data.add(peer_metadata);
  EXPECT_EQ(filter_->onData(data, /*end_stream*/ false), Network::FilterStatus::Continue);

  EXPECT_EQ(data.length(), peer_metadata.size());
  const auto peer_info = peerInfoFromFilterState();
  EXPECT_FALSE(peer_info.has_value());
}

TEST_F(PeerMetadataUpstreamFilterTest, TestPeerMetadataNotConsumedWhenMalformed) {
  initialize();
  setUpstreamHost(makeInternalListenerHost("connect_originate"));

  EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);

  std::string malformed{"well hello there, my friend!"};
  Buffer::OwnedImpl data;
  data.add(malformed);
  EXPECT_EQ(filter_->onData(data, /*end_stream*/ false), Network::FilterStatus::Continue);

  EXPECT_EQ(data.length(), malformed.size());
  const auto peer_info = peerInfoFromFilterState();
  EXPECT_FALSE(peer_info.has_value());
}

} // namespace
} // namespace PeerMetadata
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
