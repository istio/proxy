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

#include "extensions/stackdriver/metric/registry.h"
#include "extensions/stackdriver/common/constants.h"
#include "google/api/monitored_resource.pb.h"

#include <typeinfo>

namespace Extensions {
namespace Stackdriver {
namespace Metric {

using namespace Extensions::Stackdriver::Common;
using namespace opencensus::exporters::stats;
using namespace opencensus::stats;
using namespace google::api;
using wasm::common::NodeInfo;

// Gets monitored resource proto based on the type and node metadata info.
// Only two types of monitored resource could be returned: k8s_container or
// k8s_pod.
MonitoredResource getMonitoredResource(
    const std::string &monitored_resource_type,
    const NodeInfo &local_node_info) {
  google::api::MonitoredResource monitored_resource;
  monitored_resource.set_type(monitored_resource_type);
  auto platform_metadata = local_node_info.platform_metadata();
  (*monitored_resource.mutable_labels())[kProjectIDLabel] =
      platform_metadata[kGCPProjectKey];
  (*monitored_resource.mutable_labels())[kLocationLabel] =
      platform_metadata[kGCPClusterLocationKey];
  (*monitored_resource.mutable_labels())[kClusterNameLabel] =
      platform_metadata[kGCPClusterNameKey];
  (*monitored_resource.mutable_labels())[kNamespaceNameLabel] =
      local_node_info.namespace_();
  (*monitored_resource.mutable_labels())[kPodNameLabel] =
      local_node_info.name();

  if (monitored_resource_type == kPodMonitoredResource) {
    // no need to fill in container_name for pod monitored resource.
    return monitored_resource;
  }

  // Fill in container_name of k8s_container monitored resource.
  // If no container listed in NodeInfo, fill in the default container name
  // "istio-proxy".
  if (local_node_info.ports_to_containers().empty()) {
    (*monitored_resource.mutable_labels())[kContainerNameLabel] =
        kIstioProxyContainerName;
  } else {
    (*monitored_resource.mutable_labels())[kContainerNameLabel] =
        local_node_info.ports_to_containers().begin()->second;
  }
  return monitored_resource;
}

// Gets opencensus stackdriver exporter options.
StackdriverOptions getStackdriverOptions(const NodeInfo &local_node_info) {
  StackdriverOptions options;
  auto platform_metadata = local_node_info.platform_metadata();
  options.project_id = platform_metadata[kGCPProjectKey];

  // Get server and client monitored resource.
  auto server_monitored_resource =
      getMonitoredResource(kContainerMonitoredResource, local_node_info);
  auto client_monitored_resource =
      getMonitoredResource(kPodMonitoredResource, local_node_info);

  // TODO: Add per view monitored resource option and corresponding test once
  // https://github.com/envoyproxy/envoy/pull/7622 reaches istio/proxy.
  // options.monitored_resource[kServerRequestCountView] =
  // server_monitored_resource;
  // options.monitored_resource[kServerRequestBytesView] =
  // server_monitored_resource;
  // options.monitored_resource[kServerResponseBytesView] =
  // server_monitored_resource;
  // options.monitored_resource[kServerResponseLatenciesView] =
  // server_monitored_resource;
  // options.monitored_resource[kClientRequestCountView] =
  // client_monitored_resource;
  // options.monitored_resource[kClientRequestBytesView] =
  // client_monitored_resource;
  // options.monitored_resource[kClientResponseBytesView] =
  // client_monitored_resource;
  // options.monitored_resource[kClientRoundtripLatenciesView] =
  // client_monitored_resource;

  return options;
}

/*
 *  view function macros
 */
#define REGISTER_COUNT_VIEW(_v)                              \
  void register##_v##View() {                                \
    const ViewDescriptor view_descriptor =                   \
        ViewDescriptor()                                     \
            .set_name(k##_v##View)                           \
            .set_measure(k##_v##Measure)                     \
            .set_aggregation(Aggregation::Count()) ADD_TAGS; \
    View view(view_descriptor);                              \
    view_descriptor.RegisterForExport();                     \
  }

#define REGISTER_DISTRIBUTION_VIEW(_v)                              \
  void register##_v##View() {                                       \
    const ViewDescriptor view_descriptor =                          \
        ViewDescriptor()                                            \
            .set_name(k##_v##View)                                  \
            .set_measure(k##_v##Measure)                            \
            .set_aggregation(Aggregation::Distribution(             \
                BucketBoundaries::Exponential(20, 1, 2))) ADD_TAGS; \
    View view(view_descriptor);                                     \
    view_descriptor.RegisterForExport();                            \
  }

#define ADD_TAGS                                     \
  .add_column(requestOperationKey())                 \
      .add_column(requestProtocolKey())              \
      .add_column(serviceAuthenticationPolicyKey())  \
      .add_column(meshUIDKey())                      \
      .add_column(destinationServiceNameKey())       \
      .add_column(destinationServiceNamespaceKey())  \
      .add_column(destinationPortKey())              \
      .add_column(responseCodeKey())                 \
      .add_column(sourcePrincipalKey())              \
      .add_column(sourceWorkloadNameKey())           \
      .add_column(sourceWorkloadNamespaceKey())      \
      .add_column(sourceOwnerKey())                  \
      .add_column(destinationPrincipalKey())         \
      .add_column(destinationWorkloadNameKey())      \
      .add_column(destinationWorkloadNamespaceKey()) \
      .add_column(destinationOwnerKey())

// Functions to register opencensus views to export.
REGISTER_COUNT_VIEW(ServerRequestCount)
REGISTER_DISTRIBUTION_VIEW(ServerRequestBytes)
REGISTER_DISTRIBUTION_VIEW(ServerResponseBytes)
REGISTER_DISTRIBUTION_VIEW(ServerResponseLatencies)
REGISTER_COUNT_VIEW(ClientRequestCount)
REGISTER_DISTRIBUTION_VIEW(ClientRequestBytes)
REGISTER_DISTRIBUTION_VIEW(ClientResponseBytes)
REGISTER_DISTRIBUTION_VIEW(ClientRoundtripLatencies)

/*
 * measure function macros
 */
#define MEASURE_FUNC(_fn, _m, _u, _t)                   \
  Measure##_t _fn##Measure() {                          \
    static const Measure##_t measure =                  \
        Measure##_t::Register(k##_m##Measure, "", #_u); \
    return measure;                                     \
  }

// Meausre functions
MEASURE_FUNC(serverRequestCount, ServerRequestCount, 1, Int64)
MEASURE_FUNC(serverRequestBytes, ServerRequestBytes, By, Int64)
MEASURE_FUNC(serverResponseBytes, ServerResponseBytes, By, Int64)
MEASURE_FUNC(serverResponseLatencies, ServerResponseLatencies, ms, Double)
MEASURE_FUNC(clientRequestCount, ClientRequestCount, 1, Int64)
MEASURE_FUNC(clientRequestBytes, ClientRequestBytes, By, Int64)
MEASURE_FUNC(clientResponseBytes, ClientResponseBytes, By, Int64)
MEASURE_FUNC(clientRoundtripLatencies, ClientRoundtripLatencies, ms, Double)

void registerViews() {
  // Register measure first, which views depend on.
  serverRequestCountMeasure();
  serverRequestBytesMeasure();
  serverResponseBytesMeasure();
  serverResponseLatenciesMeasure();
  clientRequestCountMeasure();
  clientRequestBytesMeasure();
  clientResponseBytesMeasure();
  clientRoundtripLatenciesMeasure();

  // Register views to export;
  registerServerRequestCountView();
  registerServerRequestBytesView();
  registerServerResponseBytesView();
  registerServerResponseLatenciesView();
  registerClientRequestCountView();
  registerClientRequestBytesView();
  registerClientResponseBytesView();
  registerClientRoundtripLatenciesView();
}

/*
 * tag key function macros
 */
#define TAG_KEY_FUNC(_t, _f)                                              \
  opencensus::tags::TagKey _f##Key() {                                    \
    static const auto _t##_key = opencensus::tags::TagKey::Register(#_t); \
    return _t##_key;                                                      \
  }

// Tag key functions
TAG_KEY_FUNC(response_code, responseCode)
TAG_KEY_FUNC(request_operation, requestOperation)
TAG_KEY_FUNC(request_protocol, requestProtocol)
TAG_KEY_FUNC(service_authentication_policy, serviceAuthenticationPolicy)
TAG_KEY_FUNC(mesh_uid, meshUID)
TAG_KEY_FUNC(destination_service_name, destinationServiceName)
TAG_KEY_FUNC(destination_service_namespace, destinationServiceNamespace)
TAG_KEY_FUNC(destination_port, destinationPort)
TAG_KEY_FUNC(response_code, desponseCode)
TAG_KEY_FUNC(source_principal, sourcePrincipal)
TAG_KEY_FUNC(source_workload_name, sourceWorkloadName)
TAG_KEY_FUNC(source_workload_namespace, sourceWorkloadNamespace)
TAG_KEY_FUNC(source_owner, sourceOwner)
TAG_KEY_FUNC(destination_principal, destinationPrincipal)
TAG_KEY_FUNC(destination_workload_name, destinationWorkloadName)
TAG_KEY_FUNC(destination_workload_namespace, destinationWorkloadNamespace)
TAG_KEY_FUNC(destination_owner, destinationOwner)

}  // namespace Metric
}  // namespace Stackdriver
}  // namespace Extensions
