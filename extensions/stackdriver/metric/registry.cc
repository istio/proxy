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
#include "extensions/stackdriver/common/utils.h"
#include "google/api/monitored_resource.pb.h"
#include "grpcpp/grpcpp.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

using namespace Extensions::Stackdriver::Common;
using namespace opencensus::exporters::stats;
using namespace opencensus::stats;
using wasm::common::NodeInfo;

// Gets opencensus stackdriver exporter options.
StackdriverOptions getStackdriverOptions(
    const NodeInfo &local_node_info,
    const std::string &test_monitoring_endpoint) {
  StackdriverOptions options;
  auto platform_metadata = local_node_info.platform_metadata();
  options.project_id = platform_metadata[kGCPProjectKey];

  if (!test_monitoring_endpoint.empty()) {
    auto channel = grpc::CreateChannel(test_monitoring_endpoint,
                                       grpc::InsecureChannelCredentials());
    options.metric_service_stub =
        google::monitoring::v3::MetricService::NewStub(channel);
  }

  std::string server_type = kContainerMonitoredResource;
  std::string client_type = kPodMonitoredResource;
  auto iter = platform_metadata.find(kGCPClusterNameKey);
  if (platform_metadata.end() == iter) {
    // if there is no cluster name, then this is a gce_instance
    server_type = kGCEInstanceMonitoredResource;
    client_type = kGCEInstanceMonitoredResource;
  }

  // Get server and client monitored resource.
  google::api::MonitoredResource server_monitored_resource;
  Common::getMonitoredResource(server_type, local_node_info,
                               &server_monitored_resource);
  google::api::MonitoredResource client_monitored_resource;
  Common::getMonitoredResource(client_type, local_node_info,
                               &client_monitored_resource);
  options.per_metric_monitored_resource[kServerRequestCountView] =
      server_monitored_resource;
  options.per_metric_monitored_resource[kServerRequestBytesView] =
      server_monitored_resource;
  options.per_metric_monitored_resource[kServerResponseBytesView] =
      server_monitored_resource;
  options.per_metric_monitored_resource[kServerResponseLatenciesView] =
      server_monitored_resource;
  options.per_metric_monitored_resource[kClientRequestCountView] =
      client_monitored_resource;
  options.per_metric_monitored_resource[kClientRequestBytesView] =
      client_monitored_resource;
  options.per_metric_monitored_resource[kClientResponseBytesView] =
      client_monitored_resource;
  options.per_metric_monitored_resource[kClientRoundtripLatenciesView] =
      client_monitored_resource;
  options.metric_name_prefix = kIstioMetricPrefix;
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
