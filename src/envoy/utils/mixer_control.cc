/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "src/envoy/utils/mixer_control.h"
#include "src/envoy/utils/grpc_transport.h"

using ::istio::mixerclient::Statistics;
using ::istio::utils::AttributeName;
using ::istio::utils::LocalAttributes;

namespace Envoy {
namespace Utils {

const char NodeKey::kName[] = "NODE_NAME";
const char NodeKey::kNamespace[] = "NODE_NAMESPACE";
const char NodeKey::kIp[] = "NODE_IP";
const char NodeKey::kRegistry[] = "NODE_REGISTRY";

namespace {

// A class to wrap envoy timer for mixer client timer.
class EnvoyTimer : public ::istio::mixerclient::Timer {
 public:
  EnvoyTimer(Event::TimerPtr timer) : timer_(std::move(timer)) {}

  void Stop() override { timer_->disableTimer(); }
  void Start(int interval_ms) override {
    timer_->enableTimer(std::chrono::milliseconds(interval_ms));
  }

 private:
  Event::TimerPtr timer_;
};

// Fork of Envoy::Grpc::AsyncClientFactoryImpl, workaround for
// https://github.com/envoyproxy/envoy/issues/2762
class EnvoyGrpcAsyncClientFactory : public Grpc::AsyncClientFactory {
 public:
  EnvoyGrpcAsyncClientFactory(Upstream::ClusterManager &cm,
                              envoy::api::v2::core::GrpcService config)
      : cm_(cm), config_(config) {}

  Grpc::AsyncClientPtr create() override {
    return std::make_unique<Grpc::AsyncClientImpl>(cm_, config_);
  }

 private:
  Upstream::ClusterManager &cm_;
  envoy::api::v2::core::GrpcService config_;
};

}  // namespace

// Create all environment functions for mixerclient
void CreateEnvironment(Event::Dispatcher &dispatcher,
                       Runtime::RandomGenerator &random,
                       Grpc::AsyncClientFactory &check_client_factory,
                       Grpc::AsyncClientFactory &report_client_factory,
                       const std::string &serialized_forward_attributes,
                       ::istio::mixerclient::Environment *env) {
  env->check_transport = CheckTransport::GetFunc(check_client_factory,
                                                 Tracing::NullSpan::instance(),
                                                 serialized_forward_attributes);
  env->report_transport = ReportTransport::GetFunc(
      report_client_factory, Tracing::NullSpan::instance(),
      serialized_forward_attributes);

  env->timer_create_func = [&dispatcher](std::function<void()> timer_cb)
      -> std::unique_ptr<::istio::mixerclient::Timer> {
    return std::unique_ptr<::istio::mixerclient::Timer>(
        new EnvoyTimer(dispatcher.createTimer(timer_cb)));
  };

  env->uuid_generate_func = [&random]() -> std::string {
    return random.uuid();
  };
}

void SerializeForwardedAttributes(
    const ::istio::mixer::v1::config::client::TransportConfig &transport,
    std::string *serialized_forward_attributes) {
  if (!transport.attributes_for_mixer_proxy().attributes().empty()) {
    transport.attributes_for_mixer_proxy().SerializeToString(
        serialized_forward_attributes);
  }
}

Grpc::AsyncClientFactoryPtr GrpcClientFactoryForCluster(
    const std::string &cluster_name, Upstream::ClusterManager &cm,
    Stats::Scope &scope) {
  envoy::api::v2::core::GrpcService service;
  service.mutable_envoy_grpc()->set_cluster_name(cluster_name);

  // Workaround for https://github.com/envoyproxy/envoy/issues/2762
  UNREFERENCED_PARAMETER(scope);
  return std::make_unique<EnvoyGrpcAsyncClientFactory>(cm, service);
}

// create Local attributes object and return a pointer to it.
// Should be freed by the caller.
const LocalAttributes *CreateLocalAttributes(const LocalAttributesArgs &local) {
  ::istio::mixer::v1::Attributes ib;
  auto &inbound = (*ib.mutable_attributes());
  inbound[AttributeName::kDestinationUID].set_string_value(local.uid);
  inbound[AttributeName::kContextReporterUID].set_string_value(local.uid);
  inbound[AttributeName::kDestinationNamespace].set_string_value(local.ns);
  if (!local.ip.empty()) {
    // TODO: mjog check if destination.ip should be setup for inbound.
  }

  ::istio::mixer::v1::Attributes ob;
  auto &outbound = (*ob.mutable_attributes());
  outbound[AttributeName::kSourceUID].set_string_value(local.uid);
  outbound[AttributeName::kContextReporterUID].set_string_value(local.uid);
  outbound[AttributeName::kSourceNamespace].set_string_value(local.ns);

  ::istio::mixer::v1::Attributes fwd;
  auto &forward = (*fwd.mutable_attributes());
  forward[AttributeName::kSourceUID].set_string_value(local.uid);
  return new LocalAttributes(ib, ob, fwd);
}

// This is for compatibility with existing node ids.
// "sidecar~10.36.0.15~fortioclient-84469dc8d7-jbbxt.service-graph~service-graph.svc.cluster.local"
//  --> {proxy_type}~{ip}~{node_name}.{node_ns}~{node_domain}
bool ExtractInfoCompat(const std::string &nodeid, LocalAttributesArgs *args) {
  auto parts = StringUtil::splitToken(nodeid, "~");
  if (parts.size() < 3) {
    GOOGLE_LOG(ERROR) << "ExtractInfoCompat error len(nodeid.split(~))<3: "
                      << nodeid;
    return false;
  }

  auto ip = std::string(parts[1].begin(), parts[1].end());
  auto longname = std::string(parts[2].begin(), parts[2].end());
  auto names = StringUtil::splitToken(longname, ".");
  if (names.size() < 2) {
    GOOGLE_LOG(ERROR)
        << "ExtractInfoCompat error len(split(longname, '.')) < 3: "
        << longname;
    return false;
  }
  auto ns = std::string(names[1].begin(), names[1].end());

  args->ip = ip;
  args->ns = ns;
  args->uid = "kubernetes://" + longname;

  return true;
}

// ExtractInfo depends on NODE_NAME, NODE_NAMESPACE, NODE_IP and optional
// NODE_REG If work cannot be done, returns false.
bool ExtractInfo(const envoy::api::v2::core::Node &node,
                 LocalAttributesArgs *args) {
  const auto meta = node.metadata().fields();
  std::string name;
  if (!ReadMap(meta, NodeKey::kName, &name)) {
    GOOGLE_LOG(ERROR) << "ExtractInfo  metadata missing " << NodeKey::kName
                      << " " << node.metadata().DebugString();
    return false;
  }
  std::string ns;
  ReadMap(meta, NodeKey::kNamespace, &ns);

  std::string ip;
  ReadMap(meta, NodeKey::kIp, &ip);

  std::string reg("kubernetes");
  ReadMap(meta, NodeKey::kRegistry, &reg);

  args->ip = ip;
  args->ns = ns;
  args->uid = reg + "://" + name + "." + ns;

  return true;
}

const LocalAttributes *GenerateLocalAttributes(
    const envoy::api::v2::core::Node &node) {
  LocalAttributesArgs args;

  if (ExtractInfo(node, &args)) {
    return CreateLocalAttributes(args);
  }

  if (ExtractInfoCompat(node.id(), &args)) {
    return CreateLocalAttributes(args);
  }
  return nullptr;
}

}  // namespace Utils
}  // namespace Envoy
