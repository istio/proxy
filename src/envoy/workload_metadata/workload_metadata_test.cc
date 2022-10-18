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

#include "workload_metadata.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/router/string_accessor_impl.h"
#include "src/envoy/workload_metadata/config/workload_metadata.pb.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/stats/mocks.h"

using namespace ::istio::telemetry::workloadmetadata;
using namespace Envoy::Common;

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace WorkloadMetadata {

class FilterTest : public testing::Test {
 public:
  FilterTest() { ENVOY_LOG_MISC(info, "test"); }

 public:
  std::unique_ptr<Filter> newDefaultFilter() {
    v1::WorkloadMetadataResources resources;
    auto resource = resources.add_workload_metadata_resources();
    resource->set_instance_name("foo-pod-12345");
    resource->set_workload_name("foo");
    resource->set_canonical_name("foo-svc");
    resource->set_canonical_revision("v2beta1");
    resource->set_namespace_name("default");
    resource->add_ip_addresses("10.10.10.10");
    resource->add_ip_addresses("192.168.1.1");
    resource->add_containers("app");
    resource->add_containers("storage");

    Config config(store_, "my-cluster", resources);
    return std::make_unique<Filter>(std::make_shared<Config>(config));
  }

  void setAddressToReturn(const std::string& address) {
    callbacks_.socket_.connection_info_provider_->setRemoteAddress(
        Network::Utility::resolveUrl(address));
  }

 protected:
  Stats::IsolatedStoreImpl store_;
  NiceMock<Network::MockListenerFilterCallbacks> callbacks_;
};

TEST_F(FilterTest, OnAccept) {
  auto filter = newDefaultFilter();
  setAddressToReturn("tcp://10.10.10.10:9999");

  auto filter_state = std::make_shared<StreamInfo::FilterStateImpl>(
      StreamInfo::FilterState::LifeSpan::Connection);
  EXPECT_CALL(callbacks_, filterState())
      .WillOnce(
          Invoke([&]() -> StreamInfo::FilterState& { return *filter_state; }));

  EXPECT_EQ(filter->onAccept(callbacks_), Network::FilterStatus::Continue);
  EXPECT_TRUE(
      filter_state->hasDataWithName(Istio::Common::kSourceMetadataBaggageKey));
  auto found = filter_state->getDataReadOnly<Envoy::Router::StringAccessor>(
      Istio::Common::kSourceMetadataBaggageKey);
  EXPECT_EQ(found->asString(),
            "k8s.deployment.name=foo,k8s.cluster.name=my-cluster,k8s.namespace."
            "name=default,"
            "service.name=foo-svc,service.version=v2beta1");

  setAddressToReturn("tcp://192.168.1.1:5555");
  filter_state = std::make_shared<StreamInfo::FilterStateImpl>(
      StreamInfo::FilterState::LifeSpan::Connection);
  EXPECT_CALL(callbacks_, filterState())
      .WillOnce(
          Invoke([&]() -> StreamInfo::FilterState& { return *filter_state; }));
  EXPECT_EQ(filter->onAccept(callbacks_), Network::FilterStatus::Continue);
  EXPECT_TRUE(
      filter_state->hasDataWithName(Istio::Common::kSourceMetadataBaggageKey));

  found = filter_state->getDataReadOnly<Envoy::Router::StringAccessor>(
      Istio::Common::kSourceMetadataBaggageKey);
  EXPECT_EQ(found->asString(),
            "k8s.deployment.name=foo,k8s.cluster.name=my-cluster,k8s.namespace."
            "name=default,"
            "service.name=foo-svc,service.version=v2beta1");

  setAddressToReturn("tcp://4.22.1.1:4343");
  EXPECT_CALL(callbacks_, filterState()).Times(0);
  EXPECT_EQ(filter->onAccept(callbacks_), Network::FilterStatus::Continue);
}

}  // namespace WorkloadMetadata
}  // namespace Envoy
