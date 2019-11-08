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
using ::istio::utils::LocalNode;

namespace Envoy {
namespace Utils {

const char kNodeUID[] = "NODE_UID";
const char kNodeNamespace[] = "NODE_NAMESPACE";

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
                              envoy::api::v2::core::GrpcService config,
                              TimeSource &time_source)
      : cm_(cm), config_(config), time_source_(time_source) {}

  Grpc::RawAsyncClientPtr create() override {
    return std::make_unique<Grpc::AsyncClientImpl>(cm_, config_, time_source_);
  }

 private:
  Upstream::ClusterManager &cm_;
  envoy::api::v2::core::GrpcService config_;
  TimeSource &time_source_;
};

inline bool ReadProtoMap(
    const google::protobuf::Map<std::string, google::protobuf::Value> &meta,
    const std::string &key, std::string *val) {
  const auto it = meta.find(key);
  if (it != meta.end()) {
    *val = it->second.string_value();
    return true;
  }

  return false;
}

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
    Stats::Scope &scope, TimeSource &time_source) {
  envoy::api::v2::core::GrpcService service;
  service.mutable_envoy_grpc()->set_cluster_name(cluster_name);

  // Workaround for https://github.com/envoyproxy/envoy/issues/2762
  UNREFERENCED_PARAMETER(scope);
  return std::make_unique<EnvoyGrpcAsyncClientFactory>(cm, service,
                                                       time_source);
}

// This function is for compatibility with existing node ids.
// "sidecar~10.36.0.15~fortioclient-84469dc8d7-jbbxt.service-graph~service-graph.svc.cluster.local"
//  --> {proxy_type}~{ip}~{node_name}.{node_ns}~{node_domain}
bool ExtractInfoCompat(const std::string &nodeid, LocalNode *args) {
  auto &logger = Logger::Registry::getLog(Logger::Id::config);

  auto parts = StringUtil::splitToken(nodeid, "~");
  if (parts.size() < 3) {
    ENVOY_LOG_TO_LOGGER(
        logger, debug,
        "ExtractInfoCompat node id {} did not have the correct format:{} ",
        nodeid, "{proxy_type}~{ip}~{node_name}.{node_ns}~{node_domain} ");
    return false;
  }

  auto longname = std::string(parts[2].begin(), parts[2].end());
  auto names = StringUtil::splitToken(longname, ".");
  if (names.size() < 2) {
    ENVOY_LOG_TO_LOGGER(logger, debug,
                        "ExtractInfoCompat node_name {} must have two parts: "
                        "node_name.namespace",
                        longname);
    return false;
  }
  auto ns = std::string(names[1].begin(), names[1].end());

  args->ns = ns;
  args->uid = "kubernetes://" + longname;

  return true;
}

// ExtractInfo depends on NODE_UID, NODE_NAMESPACE
bool ExtractInfo(const envoy::api::v2::core::Node &node, LocalNode *args) {
  auto &logger = Logger::Registry::getLog(Logger::Id::config);

  const auto meta = node.metadata().fields();

  if (meta.empty()) {
    ENVOY_LOG_TO_LOGGER(logger, debug, "ExtractInfo node metadata empty: {}",
                        node.DebugString());
    return false;
  }

  std::string uid;
  if (!ReadProtoMap(meta, kNodeUID, &uid)) {
    ENVOY_LOG_TO_LOGGER(logger, debug,
                        "ExtractInfo node metadata missing:{} {}", kNodeUID,
                        node.metadata().DebugString());
    return false;
  }

  std::string ns;
  if (!ReadProtoMap(meta, kNodeNamespace, &ns)) {
    ENVOY_LOG_TO_LOGGER(logger, debug,
                        "ExtractInfo node metadata missing:{} {}",
                        kNodeNamespace, node.metadata().DebugString());
    return false;
  }

  args->ns = ns;
  args->uid = uid;

  return true;
}

bool ExtractNodeInfo(const envoy::api::v2::core::Node &node, LocalNode *args) {
  if (ExtractInfo(node, args)) {
    return true;
  }
  if (ExtractInfoCompat(node.id(), args)) {
    return true;
  }
  return false;
}

}  // namespace Utils
}  // namespace Envoy
