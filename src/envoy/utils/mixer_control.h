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

#pragma once

#include "envoy/event/dispatcher.h"
#include "envoy/local_info/local_info.h"
#include "envoy/runtime/runtime.h"
#include "envoy/upstream/cluster_manager.h"
#include "include/istio/mixerclient/client.h"
#include "include/istio/utils/attribute_names.h"
#include "include/istio/utils/attributes_builder.h"
#include "include/istio/utils/local_attributes.h"
#include "src/envoy/utils/config.h"

using ::istio::mixer::v1::Attributes;
using ::istio::mixer::v1::Attributes_AttributeValue;
using ::istio::utils::LocalAttributes;

namespace Envoy {
namespace Utils {

// Create all environment functions for mixerclient
void CreateEnvironment(Event::Dispatcher &dispatcher,
                       Runtime::RandomGenerator &random,
                       Grpc::AsyncClientFactory &check_client_factory,
                       Grpc::AsyncClientFactory &report_client_factory,
                       const std::string &serialized_forward_attributes,
                       ::istio::mixerclient::Environment *env);

void SerializeForwardedAttributes(
    const ::istio::mixer::v1::config::client::TransportConfig &transport,
    std::string *serialized_forward_attributes);

Grpc::AsyncClientFactoryPtr GrpcClientFactoryForCluster(
    const std::string &cluster_name, Upstream::ClusterManager &cm,
    Stats::Scope &scope);

// LocalAttributesArgs used internally
struct LocalAttributesArgs {
  std::string ns;
  std::string ip;
  std::string uid;
};

// return local attributes based on local info.
const LocalAttributes *GenerateLocalAttributes(
    const envoy::api::v2::core::Node &node);

const LocalAttributes *CreateLocalAttributes(const LocalAttributesArgs &local);

inline bool ReadMap(
    const google::protobuf::Map<std::string, google::protobuf::Value> &meta,
    const std::string &key, std::string *val) {
  const auto it = meta.find(key);
  if (it != meta.end()) {
    *val = it->second.string_value();
    return true;
  }
  return false;
}

inline bool ReadMap(
    const google::protobuf::Map<std::string, Attributes_AttributeValue> &meta,
    const std::string &key, std::string *val) {
  const auto it = meta.find(key);
  if (it != meta.end()) {
    *val = it->second.string_value();
    return true;
  }
  return false;
}

// NodeKey are node matadata keys that are expected to be set.
struct NodeKey {
  static const char kName[];
  static const char kNamespace[];
  static const char kIp[];
  static const char kRegistry[];
};

}  // namespace Utils
}  // namespace Envoy
