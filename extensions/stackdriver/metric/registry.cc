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

#include <fstream>
#include <sstream>

#include "extensions/stackdriver/common/constants.h"
#include "google/api/monitored_resource.pb.h"
#include "grpcpp/grpcpp.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

namespace {

class GoogleUserProjHeaderInterceptor : public grpc::experimental::Interceptor {
public:
  GoogleUserProjHeaderInterceptor(const std::string& project_id) : project_id_(project_id) {}

  virtual void Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
      auto* metadata_map = methods->GetSendInitialMetadata();
      if (metadata_map != nullptr) {
        metadata_map->insert(std::make_pair("x-goog-user-project", project_id_));
      }
    }
    methods->Proceed();
  }

private:
  const std::string& project_id_;
};

class GoogleUserProjHeaderInterceptorFactory
    : public grpc::experimental::ClientInterceptorFactoryInterface {
public:
  GoogleUserProjHeaderInterceptorFactory(const std::string& project_id) : project_id_(project_id) {}

  virtual grpc::experimental::Interceptor*
  CreateClientInterceptor(grpc::experimental::ClientRpcInfo*) override {
    return new GoogleUserProjHeaderInterceptor(project_id_);
  }

private:
  std::string project_id_;
};

} // namespace

using namespace Extensions::Stackdriver::Common;
using namespace opencensus::exporters::stats;
using namespace opencensus::stats;

// Gets opencensus stackdriver exporter options.
StackdriverOptions
getStackdriverOptions(const Wasm::Common::FlatNode& local_node_info,
                      const ::Extensions::Stackdriver::Common::StackdriverStubOption& stub_option) {
  StackdriverOptions options;
  auto platform_metadata = local_node_info.platform_metadata();
  if (platform_metadata) {
    auto project = platform_metadata->LookupByKey(kGCPProjectKey);
    if (project) {
      options.project_id = flatbuffers::GetString(project->value());
    }
  }

  auto ssl_creds_options = grpc::SslCredentialsOptions();
  if (!stub_option.test_root_pem_path.empty()) {
    std::ifstream file(stub_option.test_root_pem_path);
    if (!file.fail()) {
      std::stringstream file_string;
      file_string << file.rdbuf();
      ssl_creds_options.pem_root_certs = file_string.str();
    }
  }
  auto channel_creds = grpc::SslCredentials(ssl_creds_options);

  if (!stub_option.insecure_endpoint.empty()) {
    auto channel =
        grpc::CreateChannel(stub_option.insecure_endpoint, grpc::InsecureChannelCredentials());
    options.metric_service_stub = google::monitoring::v3::MetricService::NewStub(channel);
  } else if (!stub_option.sts_port.empty()) {
    ::grpc::experimental::StsCredentialsOptions sts_options;
    std::string token_path =
        stub_option.test_token_path.empty() ? kSTSSubjectTokenPath : stub_option.test_token_path;
    ::Extensions::Stackdriver::Common::setSTSCallCredentialOptions(
        &sts_options, stub_option.sts_port, token_path);
    auto call_creds = grpc::experimental::StsCredentials(sts_options);
    grpc::ChannelArguments args;
    std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>> creators;
    auto header_factory =
        std::make_unique<GoogleUserProjHeaderInterceptorFactory>(options.project_id);
    creators.push_back(std::move(header_factory));
    // When STS is turned on, first check if secure_endpoint is set or not,
    // which indicates whether this is for testing senario. If not set, check
    // for monitoring_endpoint override, which indicates a different SD backend
    // endpoint, such as staging.
    std::string monitoring_endpoint = stub_option.default_endpoint;
    if (!stub_option.secure_endpoint.empty()) {
      monitoring_endpoint = stub_option.secure_endpoint;
    } else if (!stub_option.monitoring_endpoint.empty()) {
      monitoring_endpoint = stub_option.monitoring_endpoint;
    }
    auto channel = ::grpc::experimental::CreateCustomChannelWithInterceptors(
        monitoring_endpoint, grpc::CompositeChannelCredentials(channel_creds, call_creds), args,
        std::move(creators));
    options.metric_service_stub = google::monitoring::v3::MetricService::NewStub(channel);
  } else if (!stub_option.secure_endpoint.empty()) {
    auto channel = grpc::CreateChannel(stub_option.secure_endpoint, channel_creds);
    options.metric_service_stub = google::monitoring::v3::MetricService::NewStub(channel);
  } else if (!stub_option.monitoring_endpoint.empty()) {
    auto channel =
        ::grpc::CreateChannel(stub_option.monitoring_endpoint, ::grpc::GoogleDefaultCredentials());
    options.metric_service_stub = google::monitoring::v3::MetricService::NewStub(channel);
  }

  std::string server_type = kContainerMonitoredResource;
  std::string client_type = kPodMonitoredResource;
  if (!platform_metadata) {
    server_type = kGenericNode;
    client_type = kGenericNode;
  } else if (!platform_metadata->LookupByKey(kGCPClusterNameKey)) {
    // if there is no cluster name key, assume it is not on kubernetes
    if (platform_metadata->LookupByKey(kGCPGCEInstanceIDKey) ||
        platform_metadata->LookupByKey(kGCECreatedByKey.data())) {
      // if there is instance ID or createdBy key, assume it is a GCE_INSTANCE
      server_type = kGCEInstanceMonitoredResource;
      client_type = kGCEInstanceMonitoredResource;
    } else {
      // absent GCE key info, use Generic Node
      server_type = kGenericNode;
      client_type = kGenericNode;
    }
  }

  // Get server and client monitored resource.
  google::api::MonitoredResource server_monitored_resource;
  Common::getMonitoredResource(server_type, local_node_info, &server_monitored_resource);
  google::api::MonitoredResource client_monitored_resource;
  Common::getMonitoredResource(client_type, local_node_info, &client_monitored_resource);
  options.per_metric_monitored_resource[kServerRequestCountView] = server_monitored_resource;
  options.per_metric_monitored_resource[kServerRequestBytesView] = server_monitored_resource;
  options.per_metric_monitored_resource[kServerResponseBytesView] = server_monitored_resource;
  options.per_metric_monitored_resource[kServerResponseLatenciesView] = server_monitored_resource;
  options.per_metric_monitored_resource[kServerConnectionsOpenCountView] =
      server_monitored_resource;
  options.per_metric_monitored_resource[kServerConnectionsCloseCountView] =
      server_monitored_resource;
  options.per_metric_monitored_resource[kServerReceivedBytesCountView] = server_monitored_resource;
  options.per_metric_monitored_resource[kServerSentBytesCountView] = server_monitored_resource;
  options.per_metric_monitored_resource[kClientRequestCountView] = client_monitored_resource;
  options.per_metric_monitored_resource[kClientRequestBytesView] = client_monitored_resource;
  options.per_metric_monitored_resource[kClientResponseBytesView] = client_monitored_resource;
  options.per_metric_monitored_resource[kClientRoundtripLatenciesView] = client_monitored_resource;
  options.per_metric_monitored_resource[kClientConnectionsOpenCountView] =
      client_monitored_resource;
  options.per_metric_monitored_resource[kClientConnectionsCloseCountView] =
      client_monitored_resource;
  options.per_metric_monitored_resource[kClientReceivedBytesCountView] = client_monitored_resource;
  options.per_metric_monitored_resource[kClientSentBytesCountView] = client_monitored_resource;

  options.metric_name_prefix = kIstioMetricPrefix;
  return options;
}

/*
 *  view function macros
 */
#define REGISTER_COUNT_VIEW(_v)                                                                    \
  void register##_v##View(absl::Duration expiry_duration,                                          \
                          std::vector<std::string> dropped_metrics) {                              \
    auto iter = std::find(dropped_metrics.begin(), dropped_metrics.end(), k##_v##View);            \
    if (iter != dropped_metrics.end()) {                                                           \
      return;                                                                                      \
    }                                                                                              \
    const ViewDescriptor view_descriptor = ViewDescriptor()                                        \
                                               .set_name(k##_v##View)                              \
                                               .set_measure(k##_v##Measure)                        \
                                               .set_expiry_duration(expiry_duration)               \
                                               .set_aggregation(Aggregation::Count()) ADD_TAGS;    \
    View view(view_descriptor);                                                                    \
    view_descriptor.RegisterForExport();                                                           \
  }

#define REGISTER_TCP_COUNT_VIEW(_v)                                                                \
  void register##_v##View(absl::Duration expiry_duration,                                          \
                          std::vector<std::string> dropped_metrics) {                              \
    auto iter = std::find(dropped_metrics.begin(), dropped_metrics.end(), k##_v##View);            \
    if (iter != dropped_metrics.end()) {                                                           \
      return;                                                                                      \
    }                                                                                              \
    const ViewDescriptor view_descriptor = ViewDescriptor()                                        \
                                               .set_name(k##_v##View)                              \
                                               .set_measure(k##_v##Measure)                        \
                                               .set_expiry_duration(expiry_duration)               \
                                               .set_aggregation(Aggregation::Count())              \
                                                   ADD_COMMON_TAGS;                                \
    View view(view_descriptor);                                                                    \
    view_descriptor.RegisterForExport();                                                           \
  }

#define REGISTER_TCP_SUM_VIEW(_v)                                                                  \
  void register##_v##View(absl::Duration expiry_duration,                                          \
                          std::vector<std::string> dropped_metrics) {                              \
    auto iter = std::find(dropped_metrics.begin(), dropped_metrics.end(), k##_v##View);            \
    if (iter != dropped_metrics.end()) {                                                           \
      return;                                                                                      \
    }                                                                                              \
    const ViewDescriptor view_descriptor = ViewDescriptor()                                        \
                                               .set_name(k##_v##View)                              \
                                               .set_measure(k##_v##Measure)                        \
                                               .set_expiry_duration(expiry_duration)               \
                                               .set_aggregation(Aggregation::Sum())                \
                                                   ADD_COMMON_TAGS;                                \
    View view(view_descriptor);                                                                    \
    view_descriptor.RegisterForExport();                                                           \
  }

#define REGISTER_DISTRIBUTION_VIEW(_v)                                                             \
  void register##_v##View(absl::Duration expiry_duration,                                          \
                          std::vector<std::string> dropped_metrics) {                              \
    auto iter = std::find(dropped_metrics.begin(), dropped_metrics.end(), k##_v##View);            \
    if (iter != dropped_metrics.end()) {                                                           \
      return;                                                                                      \
    }                                                                                              \
    const ViewDescriptor view_descriptor =                                                         \
        ViewDescriptor()                                                                           \
            .set_name(k##_v##View)                                                                 \
            .set_measure(k##_v##Measure)                                                           \
            .set_expiry_duration(expiry_duration)                                                  \
            .set_aggregation(Aggregation::Distribution(BucketBoundaries::Exponential(20, 1, 2)))   \
                ADD_TAGS;                                                                          \
    View view(view_descriptor);                                                                    \
    view_descriptor.RegisterForExport();                                                           \
  }

#define REGISTER_BYTES_DISTRIBUTION_VIEW(_v)                                                       \
  void register##_v##View(absl::Duration expiry_duration,                                          \
                          std::vector<std::string> dropped_metrics) {                              \
    auto iter = std::find(dropped_metrics.begin(), dropped_metrics.end(), k##_v##View);            \
    if (iter != dropped_metrics.end()) {                                                           \
      return;                                                                                      \
    }                                                                                              \
    const ViewDescriptor view_descriptor =                                                         \
        ViewDescriptor()                                                                           \
            .set_name(k##_v##View)                                                                 \
            .set_measure(k##_v##Measure)                                                           \
            .set_expiry_duration(expiry_duration)                                                  \
            .set_aggregation(Aggregation::Distribution(BucketBoundaries::Exponential(7, 1, 10)))   \
                ADD_TAGS;                                                                          \
    View view(view_descriptor);                                                                    \
    view_descriptor.RegisterForExport();                                                           \
  }

#define ADD_TAGS ADD_COMMON_TAGS ADD_HTTP_GRPC_TAGS

#define ADD_HTTP_GRPC_TAGS                                                                         \
  .add_column(requestOperationKey())                                                               \
      .add_column(responseCodeKey())                                                               \
      .add_column(apiVersionKey())                                                                 \
      .add_column(apiNameKey())

#define ADD_COMMON_TAGS                                                                            \
  .add_column(requestProtocolKey())                                                                \
      .add_column(serviceAuthenticationPolicyKey())                                                \
      .add_column(meshUIDKey())                                                                    \
      .add_column(destinationServiceNameKey())                                                     \
      .add_column(destinationServiceNamespaceKey())                                                \
      .add_column(destinationPortKey())                                                            \
      .add_column(sourcePrincipalKey())                                                            \
      .add_column(sourceWorkloadNameKey())                                                         \
      .add_column(sourceWorkloadNamespaceKey())                                                    \
      .add_column(sourceOwnerKey())                                                                \
      .add_column(destinationPrincipalKey())                                                       \
      .add_column(destinationWorkloadNameKey())                                                    \
      .add_column(destinationWorkloadNamespaceKey())                                               \
      .add_column(destinationOwnerKey())                                                           \
      .add_column(destinationCanonicalServiceNameKey())                                            \
      .add_column(destinationCanonicalServiceNamespaceKey())                                       \
      .add_column(sourceCanonicalServiceNameKey())                                                 \
      .add_column(sourceCanonicalServiceNamespaceKey())                                            \
      .add_column(destinationCanonicalRevisionKey())                                               \
      .add_column(sourceCanonicalRevisionKey())                                                    \
      .add_column(proxyVersionKey())

// Functions to register opencensus views to export.
REGISTER_COUNT_VIEW(ServerRequestCount)
REGISTER_BYTES_DISTRIBUTION_VIEW(ServerRequestBytes)
REGISTER_BYTES_DISTRIBUTION_VIEW(ServerResponseBytes)
REGISTER_DISTRIBUTION_VIEW(ServerResponseLatencies)
REGISTER_COUNT_VIEW(ClientRequestCount)
REGISTER_BYTES_DISTRIBUTION_VIEW(ClientRequestBytes)
REGISTER_BYTES_DISTRIBUTION_VIEW(ClientResponseBytes)
REGISTER_DISTRIBUTION_VIEW(ClientRoundtripLatencies)
REGISTER_TCP_COUNT_VIEW(ServerConnectionsOpenCount)
REGISTER_TCP_COUNT_VIEW(ServerConnectionsCloseCount)
REGISTER_TCP_SUM_VIEW(ServerReceivedBytesCount)
REGISTER_TCP_SUM_VIEW(ServerSentBytesCount)
REGISTER_TCP_COUNT_VIEW(ClientConnectionsOpenCount)
REGISTER_TCP_COUNT_VIEW(ClientConnectionsCloseCount)
REGISTER_TCP_SUM_VIEW(ClientReceivedBytesCount)
REGISTER_TCP_SUM_VIEW(ClientSentBytesCount)

/*
 * measure function macros
 */
#define MEASURE_FUNC(_fn, _m, _u, _t)                                                              \
  Measure##_t _fn##Measure() {                                                                     \
    static const Measure##_t measure = Measure##_t::Register(k##_m##Measure, "", #_u);             \
    return measure;                                                                                \
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
MEASURE_FUNC(serverConnectionsOpenCount, ServerConnectionsOpenCount, 1, Int64)
MEASURE_FUNC(serverConnectionsCloseCount, ServerConnectionsCloseCount, 1, Int64)
MEASURE_FUNC(serverReceivedBytesCount, ServerReceivedBytesCount, By, Int64)
MEASURE_FUNC(serverSentBytesCount, ServerSentBytesCount, By, Int64)
MEASURE_FUNC(clientConnectionsOpenCount, ClientConnectionsOpenCount, 1, Int64)
MEASURE_FUNC(clientConnectionsCloseCount, ClientConnectionsCloseCount, 1, Int64)
MEASURE_FUNC(clientReceivedBytesCount, ClientReceivedBytesCount, By, Int64)
MEASURE_FUNC(clientSentBytesCount, ClientSentBytesCount, By, Int64)

void registerViews(absl::Duration expiry_duration,
                   const std::vector<std::string>& dropped_metrics) {
  // Register measure first, which views depend on.
  serverRequestCountMeasure();
  serverRequestBytesMeasure();
  serverResponseBytesMeasure();
  serverResponseLatenciesMeasure();
  clientRequestCountMeasure();
  clientRequestBytesMeasure();
  clientResponseBytesMeasure();
  clientRoundtripLatenciesMeasure();
  serverConnectionsOpenCountMeasure();
  serverConnectionsCloseCountMeasure();
  serverReceivedBytesCountMeasure();
  serverSentBytesCountMeasure();
  clientConnectionsOpenCountMeasure();
  clientConnectionsCloseCountMeasure();
  clientReceivedBytesCountMeasure();
  clientSentBytesCountMeasure();

  // Register views to export;
  registerServerRequestCountView(expiry_duration, dropped_metrics);
  registerServerRequestBytesView(expiry_duration, dropped_metrics);
  registerServerResponseBytesView(expiry_duration, dropped_metrics);
  registerServerResponseLatenciesView(expiry_duration, dropped_metrics);
  registerClientRequestCountView(expiry_duration, dropped_metrics);
  registerClientRequestBytesView(expiry_duration, dropped_metrics);
  registerClientResponseBytesView(expiry_duration, dropped_metrics);
  registerClientRoundtripLatenciesView(expiry_duration, dropped_metrics);
  registerServerConnectionsOpenCountView(expiry_duration, dropped_metrics);
  registerServerConnectionsCloseCountView(expiry_duration, dropped_metrics);
  registerServerReceivedBytesCountView(expiry_duration, dropped_metrics);
  registerServerSentBytesCountView(expiry_duration, dropped_metrics);
  registerClientConnectionsOpenCountView(expiry_duration, dropped_metrics);
  registerClientConnectionsCloseCountView(expiry_duration, dropped_metrics);
  registerClientReceivedBytesCountView(expiry_duration, dropped_metrics);
  registerClientSentBytesCountView(expiry_duration, dropped_metrics);
}

void dropViews(const std::vector<std::string>& dropped_metrics) {
  for (const auto& metric : dropped_metrics) {
    opencensus::stats::StatsExporter::RemoveView(metric);
  }
}

/*
 * tag key function macros
 */
#define TAG_KEY_FUNC(_t, _f)                                                                       \
  opencensus::tags::TagKey _f##Key() {                                                             \
    static const auto _t##_key = opencensus::tags::TagKey::Register(#_t);                          \
    return _t##_key;                                                                               \
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
TAG_KEY_FUNC(source_canonical_service_name, sourceCanonicalServiceName)
TAG_KEY_FUNC(source_canonical_service_namespace, sourceCanonicalServiceNamespace)
TAG_KEY_FUNC(destination_canonical_service_name, destinationCanonicalServiceName)
TAG_KEY_FUNC(destination_canonical_service_namespace, destinationCanonicalServiceNamespace)
TAG_KEY_FUNC(source_canonical_revision, sourceCanonicalRevision)
TAG_KEY_FUNC(destination_canonical_revision, destinationCanonicalRevision)
TAG_KEY_FUNC(api_name, apiName)
TAG_KEY_FUNC(api_version, apiVersion)
TAG_KEY_FUNC(proxy_version, proxyVersion)

} // namespace Metric
} // namespace Stackdriver
} // namespace Extensions
