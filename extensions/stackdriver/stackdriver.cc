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

#include "extensions/stackdriver/stackdriver.h"

#include <google/protobuf/util/json_util.h>

#include <random>
#include <string>
#include <unordered_map>

#include "extensions/common/proto_util.h"
#include "extensions/common/util.h"
#include "extensions/stackdriver/log/exporter.h"
#include "extensions/stackdriver/metric/registry.h"
#include "google/protobuf/util/time_util.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
#endif

#include "contrib/proxy_expr.h"

namespace Stackdriver {

using namespace opencensus::exporters::stats;
using namespace google::protobuf::util;
using namespace ::Extensions::Stackdriver::Common;
using namespace ::Extensions::Stackdriver::Metric;
using Extensions::Stackdriver::Log::ExporterImpl;
using ::Extensions::Stackdriver::Log::Logger;
using stackdriver::config::v1alpha1::PluginConfig;
using ::Wasm::Common::kDownstreamMetadataIdKey;
using ::Wasm::Common::kDownstreamMetadataKey;
using ::Wasm::Common::kUpstreamMetadataIdKey;
using ::Wasm::Common::kUpstreamMetadataKey;
using ::Wasm::Common::RequestInfo;
using ::Wasm::Common::TCPConnectionState;

constexpr char kStackdriverExporter[] = "stackdriver_exporter";
constexpr char kExporterRegistered[] = "registered";
constexpr int kDefaultTickerMilliseconds = 10000; // 10s

namespace {

constexpr char kRbacAccessAllowed[] = "AuthzAllowed";
constexpr char kRbacAccessDenied[] = "AuthzDenied";
constexpr char kRBACHttpFilterName[] = "envoy.filters.http.rbac";
constexpr char kRBACNetworkFilterName[] = "envoy.filters.network.rbac";
constexpr char kDryRunDenyShadowEngineResult[] = "istio_dry_run_deny_shadow_engine_result";
constexpr char kDryRunAllowShadowEngineResult[] = "istio_dry_run_allow_shadow_engine_result";
constexpr char kDryRunDenyShadowEffectiveId[] = "istio_dry_run_deny_shadow_effective_policy_id";
constexpr char kDryRunAllowShadowEffectiveId[] = "istio_dry_run_allow_shadow_effective_policy_id";

// Get metric export interval from node metadata. Returns 60 seconds if interval
// is not found in metadata.
int getMonitoringExportInterval() {
  std::string interval_s = "";
  if (getValue({"node", "metadata", kMonitoringExportIntervalKey}, &interval_s)) {
    return std::stoi(interval_s);
  }
  return 60;
}

// Get proxy timer interval from node metadata in milliseconds. Returns 10
// seconds if interval is not found in metadata.
int getProxyTickerIntervalMilliseconds() {
  std::string interval_s = "";
  if (getValue({"node", "metadata", kProxyTickerIntervalKey}, &interval_s)) {
    return std::stoi(interval_s) * 1000;
  }
  return kDefaultTickerMilliseconds;
}

// Get logging export interval from node metadata in nanoseconds. Returns 60
// seconds if interval is not found in metadata.
long int getTcpLogEntryTimeoutNanoseconds() {
  std::string interval_s = "";
  if (getValue({"node", "metadata", kTcpLogEntryTimeoutKey}, &interval_s)) {
    return std::stoi(interval_s) * 1000000000;
  }
  return kDefaultTcpLogEntryTimeoutNanoseconds;
}

// Get port of security token exchange server from node metadata, if not
// provided or "0" is provided, emtpy will be returned.
std::string getSTSPort() {
  std::string sts_port;
  if (getValue({"node", "metadata", kSTSPortKey}, &sts_port) && sts_port != "0") {
    return sts_port;
  }
  return "";
}

// Get file name for the token test override.
std::string getTokenFile() {
  std::string token_file;
  if (!getValue({"node", "metadata", kTokenFile}, &token_file)) {
    return "";
  }
  return token_file;
}

// Get file name for the root CA PEM file test override.
std::string getCACertFile() {
  std::string ca_cert_file;
  if (!getValue({"node", "metadata", kCACertFile}, &ca_cert_file)) {
    return "";
  }
  return ca_cert_file;
}

// Get secure stackdriver endpoint for e2e testing.
std::string getSecureEndpoint() {
  std::string secure_endpoint;
  if (!getValue({"node", "metadata", kSecureStackdriverEndpointKey}, &secure_endpoint)) {
    return "";
  }
  return secure_endpoint;
}

// Get insecure stackdriver endpoint for e2e testing.
std::string getInsecureEndpoint() {
  std::string insecure_endpoint;
  if (!getValue({"node", "metadata", kInsecureStackdriverEndpointKey}, &insecure_endpoint)) {
    return "";
  }
  return insecure_endpoint;
}

// Get GCP monitoring endpoint. When this is provided, it will override the
// default production endpoint. This should be used to test staging monitoring
// endpoint.
std::string getMonitoringEndpoint() {
  std::string monitoring_endpoint;
  if (!getValue({"node", "metadata", kMonitoringEndpointKey}, &monitoring_endpoint)) {
    return "";
  }
  return monitoring_endpoint;
}

// Get GCP project number.
std::string getProjectNumber() {
  std::string project_number;
  if (!getValue({"node", "metadata", "PLATFORM_METADATA", kGCPProjectNumberKey}, &project_number)) {
    return "";
  }
  return project_number;
}

absl::Duration getMetricExpiryDuration(const stackdriver::config::v1alpha1::PluginConfig& config) {
  if (!config.has_metric_expiry_duration()) {
    return absl::ZeroDuration();
  }
  auto& duration = config.metric_expiry_duration();
  return absl::Seconds(duration.seconds()) + absl::Nanoseconds(duration.nanos());
}

std::vector<std::string>
getDroppedMetrics(const stackdriver::config::v1alpha1::PluginConfig& config) {
  std::vector<std::string> dropped_metrics;
  for (const auto& override : config.metrics_overrides()) {
    if (override.second.drop()) {
      dropped_metrics.push_back(override.first);
    }
  }
  return dropped_metrics;
}

bool isAllowedOverride(std::string metric, std::string tag) {
  for (const auto& label : kDefinedLabels) {
    if (label == tag) {
      return true;
    }
  }

  if (absl::StrContains(metric, "connection_") || absl::StrContains(metric, "bytes_count")) {
    // short-circuit for TCP metrics
    return false;
  }

  for (const auto& label : kHttpDefinedLabels) {
    if (label == tag) {
      return true;
    }
  }
  return false;
}

void clearTcpMetrics(::Wasm::Common::RequestInfo& request_info) {
  request_info.tcp_connections_opened = 0;
  request_info.tcp_sent_bytes = 0;
  request_info.tcp_received_bytes = 0;
}

// Get local node metadata. If mesh id is not filled or does not exist,
// fall back to default format `proj-<project-number>`.
flatbuffers::DetachedBuffer getLocalNodeMetadata() {
  google::protobuf::Struct node;
  auto local_node_info = ::Wasm::Common::extractLocalNodeFlatBuffer();
  ::Wasm::Common::extractStructFromNodeFlatBuffer(
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(local_node_info.data()), &node);
  const auto mesh_id_it = node.fields().find("MESH_ID");
  if (mesh_id_it != node.fields().end() && !mesh_id_it->second.string_value().empty() &&
      absl::StartsWith(mesh_id_it->second.string_value(), "proj-")) {
    // do nothing
  } else {
    // Insert or update mesh id to default format as it is missing, empty, or
    // not properly set.
    auto project_number = getProjectNumber();
    auto* mesh_id_field = (*node.mutable_fields())["MESH_ID"].mutable_string_value();
    if (!project_number.empty()) {
      *mesh_id_field = absl::StrCat("proj-", project_number);
    }
  }
  return ::Wasm::Common::extractNodeFlatBufferFromStruct(node);
}

bool extractAuthzPolicyName(const std::string& policy, std::string& out_namespace,
                            std::string& out_name, std::string& out_rule) {
  // The policy has format "ns[foo]-policy[httpbin-deny]-rule[0]".
  if (absl::StartsWith(policy, "ns[") && absl::EndsWith(policy, "]")) {
    std::string sepPolicy = "]-policy[";
    std::size_t beginNs = 3;
    std::size_t endNs = policy.find(sepPolicy, beginNs);
    if (endNs == std::string::npos) {
      return false;
    }
    std::string sepNs = "]-rule[";
    std::size_t beginName = endNs + sepPolicy.size();
    std::size_t endName = policy.find(sepNs, beginName);
    if (endName == std::string::npos) {
      return false;
    }
    std::size_t beginRule = endName + sepNs.size();
    std::size_t endRule = policy.size() - 1;

    out_namespace = policy.substr(beginNs, endNs - beginNs);
    out_name = policy.substr(beginName, endName - beginName);
    out_rule = policy.substr(beginRule, endRule - beginRule);
    return true;
  }

  return false;
}

void fillAuthzDryRunInfo(std::unordered_map<std::string, std::string>& extra_labels) {
  auto md = getProperty({"metadata", "filter_metadata", kRBACHttpFilterName});
  if (!md.has_value()) {
    md = getProperty({"metadata", "filter_metadata", kRBACNetworkFilterName});
    if (!md.has_value()) {
      LOG_DEBUG("RBAC metadata not found");
      return;
    }
  }

  bool shadow_deny_result = true;
  bool shadow_allow_result = true;
  bool has_shadow_metadata = false;
  std::string shadow_deny_policy = "";
  std::string shadow_allow_policy = "";
  for (const auto& [key, val] : md.value()->pairs()) {
    LOG_DEBUG(absl::StrCat("RBAC metadata found: key=", Wasm::Common::toAbslStringView(key),
                           ", value=", Wasm::Common::toAbslStringView(val)));
    if (key == kDryRunDenyShadowEngineResult) {
      shadow_deny_result = (val == "allowed");
    } else if (key == kDryRunAllowShadowEngineResult) {
      shadow_allow_result = (val == "allowed");
    } else if (key == kDryRunDenyShadowEffectiveId) {
      shadow_deny_policy = val;
    } else if (key == kDryRunAllowShadowEffectiveId) {
      shadow_allow_policy = val;
    } else {
      continue;
    }
    has_shadow_metadata = true;
  }

  if (!has_shadow_metadata) {
    LOG_DEBUG("RBAC dry-run metadata not found");
    return;
  }

  bool shadow_result = false;
  std::string shadow_effective_policy = "";
  if (shadow_deny_result && shadow_allow_result) {
    // If allowed by both DENY and ALLOW policy, the final shadow_result should
    // be true (allow) and the shadow_effective_policy should be from the ALLOW
    // policy.
    shadow_result = true;
    shadow_effective_policy = shadow_allow_policy;
    LOG_DEBUG("RBAC dry-run result: allowed");
  } else {
    // If denied by either DENY or ALLOW policy, the final shadow_reulst should
    // be false (denied).
    shadow_result = false;
    if (!shadow_deny_result) {
      // If denied by DENY policy, the shadow_effective_policy should be from
      // the DENY policy.
      shadow_effective_policy = shadow_deny_policy;
      LOG_DEBUG("RBAC dry-run result: denied by DENY policy");
    } else {
      // If denied by ALLOW policy, the shadow_effective_policy shold be from
      // the ALLOW policy.
      shadow_effective_policy = shadow_allow_policy;
      LOG_DEBUG("RBAC dry-run result: denied by ALLOW policy");
    }
  }

  extra_labels["dry_run_result"] = shadow_result ? kRbacAccessAllowed : kRbacAccessDenied;
  std::string policy_namespace = "";
  std::string policy_name = "";
  std::string policy_rule = "";
  if (extractAuthzPolicyName(shadow_effective_policy, policy_namespace, policy_name, policy_rule)) {
    extra_labels["dry_run_policy_name"] = absl::StrCat(policy_namespace, ".", policy_name);
    extra_labels["dry_run_policy_rule"] = policy_rule;
    LOG_DEBUG(absl::StrCat("RBAC dry-run matched policy: ns=", policy_namespace,
                           ", name=", policy_name, ", rule=", policy_rule));
  }
}

} // namespace

// onConfigure == false makes the proxy crash.
// Only policy plugins should return false.
bool StackdriverRootContext::onConfigure(size_t size) {
  initialized_ = configure(size);
  return true;
}

bool StackdriverRootContext::initializeLogFilter() {
  uint32_t token = 0;
  if (config_.access_logging_filter_expression() == "") {
    log_filter_token_ = token;
    return true;
  }

  if (createExpression(config_.access_logging_filter_expression(), &token) != WasmResult::Ok) {
    LOG_TRACE(absl::StrCat("cannot create an filter expression: " +
                           config_.access_logging_filter_expression()));
    return false;
  }
  log_filter_token_ = token;
  return true;
}

bool StackdriverRootContext::configure(size_t configuration_size) {
  // onStart is called prior to onConfigure
  int proxy_tick_ms = getProxyTickerIntervalMilliseconds();
  proxy_set_tick_period_milliseconds(getProxyTickerIntervalMilliseconds());
  // Parse configuration JSON string.
  std::string configuration = "{}";
  if (configuration_size > 0) {
    auto configuration_data =
        getBufferBytes(WasmBufferType::PluginConfiguration, 0, configuration_size);
    configuration = configuration_data->toString();
  }

  // TODO: add config validation to reject the listener if project id is not in
  // metadata. Parse configuration JSON string.
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  const auto status = JsonStringToMessage(configuration, &config_, json_options);
  if (!status.ok()) {
    logWarn("Cannot parse Stackdriver plugin configuration JSON string " + configuration + ", " +
            std::string(status.message()));
    return false;
  }
  local_node_info_ = getLocalNodeMetadata();

  if (config_.has_log_report_duration()) {
    log_report_duration_nanos_ =
        ::google::protobuf::util::TimeUtil::DurationToNanoseconds(config_.log_report_duration());
    long int proxy_tick_ns = proxy_tick_ms * 1000;
    if (log_report_duration_nanos_ < (proxy_tick_ns) ||
        log_report_duration_nanos_ % proxy_tick_ns != 0) {
      logWarn(absl::StrCat("The duration set is less than or not a multiple of default timer's "
                           "period. Default Timer MS: ",
                           proxy_tick_ms,
                           " Lod Duration Nanosecond: ", log_report_duration_nanos_));
    }
  }

  direction_ = ::Wasm::Common::getTrafficDirection();
  use_host_header_fallback_ = !config_.disable_host_header_fallback();
  const ::Wasm::Common::FlatNode& local_node =
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(local_node_info_.data());

  // Common stackdriver stub option for logging and monitoring.
  ::Extensions::Stackdriver::Common::StackdriverStubOption stub_option;
  stub_option.sts_port = getSTSPort();
  stub_option.test_token_path = getTokenFile();
  stub_option.test_root_pem_path = getCACertFile();
  stub_option.secure_endpoint = getSecureEndpoint();
  stub_option.insecure_endpoint = getInsecureEndpoint();
  stub_option.monitoring_endpoint = getMonitoringEndpoint();
  stub_option.enable_log_compression =
      config_.has_enable_log_compression() && config_.enable_log_compression().value();
  const auto platform_metadata = local_node.platform_metadata();
  if (platform_metadata) {
    const auto project_iter = platform_metadata->LookupByKey(kGCPProjectKey);
    if (project_iter) {
      stub_option.project_id = flatbuffers::GetString(project_iter->value());
    }
  }

  if (enableAccessLog()) {
    std::unordered_map<std::string, std::string> extra_labels;
    cleanupExpressions();
    cleanupLogFilter();
    if (!initializeLogFilter()) {
      LOG_WARN("Could not build filter expression for logging.");
    }

    if (config_.has_custom_log_config()) {
      for (const auto& dimension : config_.custom_log_config().dimensions()) {
        uint32_t token;
        if (createExpression(dimension.second, &token) != WasmResult::Ok) {
          LOG_TRACE(absl::StrCat("Could not create expression for ", dimension.second));
          continue;
        }
        expressions_.push_back({token, dimension.first, dimension.second});
      }
    }
    // logger should only be initiated once, for now there is no reason to
    // recreate logger because of config update.
    if (!logger_) {
      auto logging_stub_option = stub_option;
      logging_stub_option.default_endpoint = kLoggingService;
      auto exporter = std::make_unique<ExporterImpl>(this, logging_stub_option);
      // logger takes ownership of exporter.
      if (config_.max_log_batch_size_in_bytes() > 0) {
        logger_ = std::make_unique<Logger>(local_node, std::move(exporter), extra_labels,
                                           config_.max_log_batch_size_in_bytes());
      } else {
        logger_ = std::make_unique<Logger>(local_node, std::move(exporter), extra_labels);
      }
    }
    tcp_log_entry_timeout_ = getTcpLogEntryTimeoutNanoseconds();
  }

  // Extract metric tags expressions
  cleanupMetricsExpressions();
  for (const auto& override : config_.metrics_overrides()) {
    for (const auto& tag : override.second.tag_overrides()) {
      if (!isAllowedOverride(override.first, tag.first)) {
        LOG_WARN(absl::StrCat("cannot use tag \"", tag.first, "\" in metric \"", override.first,
                              "\"; ignoring override"));
        continue;
      }
      uint32_t token;
      if (createExpression(tag.second, &token) != WasmResult::Ok) {
        LOG_WARN(absl::StrCat("Could not create expression: \"", tag.second, "\" for tag \"",
                              tag.first, "\" on metric \"", override.first,
                              "\"; ignoring override"));
        continue;
      }
      const auto& tag_key = ::opencensus::tags::TagKey::Register(tag.first);
      metrics_expressions_.push_back({token, override.first, tag_key, tag.second});
    }
  }

  // Register OC Stackdriver exporter and views to be exported.
  // Note exporter and views are global singleton so they should only be
  // registered once.
  WasmDataPtr registered;
  if (WasmResult::Ok == getSharedData(kStackdriverExporter, &registered)) {
    return true;
  }

  setSharedData(kStackdriverExporter, kExporterRegistered);
  auto monitoring_stub_option = stub_option;
  monitoring_stub_option.default_endpoint = kMonitoringService;
  opencensus::exporters::stats::StackdriverExporter::Register(
      getStackdriverOptions(local_node, monitoring_stub_option));
  opencensus::stats::StatsExporter::SetInterval(absl::Seconds(getMonitoringExportInterval()));

  // Register opencensus measures and views.
  auto dropped = getDroppedMetrics(config_);
  dropViews(dropped);
  registerViews(getMetricExpiryDuration(config_), dropped);

  return true;
}

bool StackdriverRootContext::onStart(size_t) { return true; }

void StackdriverRootContext::onTick() {
  auto cur = static_cast<long int>(getCurrentTimeNanoseconds());

  for (auto const& item : tcp_request_queue_) {
    // requestinfo is null, so continue.
    if (item.second == nullptr) {
      continue;
    }
    Context* context = getContext(item.first);
    if (context == nullptr) {
      continue;
    }
    context->setEffectiveContext();
    if (recordTCP(item.first)) {
      // Clear existing data in TCP metrics, so that we don't double count the
      // metrics.
      clearTcpMetrics(*(item.second->request_info));
    }
  }

  if (enableAccessLog() && (cur - last_log_report_call_nanos_ > log_report_duration_nanos_)) {
    logger_->exportLogEntry(/* is_on_done= */ false);
    last_log_report_call_nanos_ = cur;
  }
}

bool StackdriverRootContext::onDone() {
  bool done = true;
  // Check if logger is empty. In base Wasm VM, only onStart and onDone are
  // called, but onConfigure is not triggered. onConfigure is only triggered in
  // thread local VM, which makes it possible that logger_ is empty ptr even
  // when logging is enabled.
  if (logger_ && enableAccessLog() && logger_->exportLogEntry(/* is_on_done= */ true)) {
    done = false;
  }
  for (auto const& item : tcp_request_queue_) {
    // requestinfo is null, so continue.
    if (item.second == nullptr) {
      continue;
    }
    recordTCP(item.first);
  }
  tcp_request_queue_.clear();
  cleanupExpressions();
  cleanupMetricsExpressions();
  cleanupLogFilter();
  return done;
}

void StackdriverRootContext::record() {
  const bool outbound = isOutbound();
  Wasm::Common::PeerNodeInfo peer_node_info(
      {outbound ? kUpstreamMetadataIdKey : kDownstreamMetadataIdKey},
      {outbound ? kUpstreamMetadataKey : kDownstreamMetadataKey});
  const ::Wasm::Common::FlatNode& local_node = getLocalNode();

  ::Wasm::Common::RequestInfo request_info;
  ::Wasm::Common::populateHTTPRequestInfo(outbound, useHostHeaderFallback(), &request_info);
  override_map overrides;
  evaluateMetricsExpressions(overrides);
  ::Extensions::Stackdriver::Metric::record(outbound, local_node, peer_node_info.get(),
                                            request_info, !config_.disable_http_size_metrics(),
                                            overrides);
  bool extended_info_populated = false;
  if ((enableAllAccessLog() ||
       (enableAccessLogOnError() && (request_info.response_code >= 400 ||
                                     request_info.response_flag != ::Wasm::Common::NONE))) &&
      shouldLogThisRequest(request_info) && evaluateLogFilter()) {
    ::Wasm::Common::populateExtendedHTTPRequestInfo(&request_info);
    std::unordered_map<std::string, std::string> extra_labels;
    evaluateExpressions(extra_labels);
    extended_info_populated = true;
    fillAuthzDryRunInfo(extra_labels);
    logger_->addLogEntry(request_info, peer_node_info.get(), extra_labels, outbound,
                         false /* audit */);
  }

  // TODO(dougreid): should Audits override log filters? I believe so. At this
  // time, we won't apply logging filters to audit logs.
  if (enableAuditLog() && shouldAuditThisRequest()) {
    if (!extended_info_populated) {
      ::Wasm::Common::populateExtendedHTTPRequestInfo(&request_info);
    }
    logger_->addLogEntry(request_info, peer_node_info.get(), {}, outbound, true /* audit */);
  }
}

bool StackdriverRootContext::recordTCP(uint32_t id) {
  const bool outbound = isOutbound();
  Wasm::Common::PeerNodeInfo peer_node_info(
      {outbound ? kUpstreamMetadataIdKey : kDownstreamMetadataIdKey},
      {outbound ? kUpstreamMetadataKey : kDownstreamMetadataKey});
  const ::Wasm::Common::FlatNode& local_node = getLocalNode();

  auto req_iter = tcp_request_queue_.find(id);
  if (req_iter == tcp_request_queue_.end() || req_iter->second == nullptr) {
    return false;
  }
  StackdriverRootContext::TcpRecordInfo& record_info = *(req_iter->second);
  ::Wasm::Common::RequestInfo& request_info = *(record_info.request_info);

  // For TCP, if peer metadata is not available, peer id is set as not found.
  // Otherwise, we wait for metadata exchange to happen before we report  any
  // metric before a timeout.
  // We keep waiting if response flags is zero, as that implies, there has
  // been no error in connection.
  uint64_t response_flags = 0;
  getValue({"response", "flags"}, &response_flags);
  auto cur = static_cast<long int>(proxy_wasm::null_plugin::getCurrentTimeNanoseconds());
  bool waiting_for_metadata = peer_node_info.maybeWaiting();
  bool no_error = response_flags == 0;
  bool log_open_on_timeout = !record_info.tcp_open_entry_logged &&
                             (cur - request_info.start_time) > tcp_log_entry_timeout_;
  if (waiting_for_metadata && no_error && !log_open_on_timeout) {
    return false;
  }
  if (!request_info.is_populated) {
    ::Wasm::Common::populateTCPRequestInfo(outbound, &request_info);
  }
  // Record TCP Metrics.
  override_map overrides;
  evaluateMetricsExpressions(overrides);
  ::Extensions::Stackdriver::Metric::recordTCP(outbound, local_node, peer_node_info.get(),
                                               request_info, overrides);
  bool extended_info_populated = false;
  // Add LogEntry to Logger. Log Entries are batched and sent on timer
  // to Stackdriver Logging Service.
  if (!record_info.log_filter_evaluated) {
    record_info.log_connection = evaluateLogFilter();
    record_info.log_filter_evaluated = true;
  }
  if ((enableAllAccessLog() || (enableAccessLogOnError() && !no_error)) &&
      record_info.log_connection) {
    ::Wasm::Common::populateExtendedRequestInfo(&request_info);
    extended_info_populated = true;
    if (!record_info.expressions_evaluated) {
      evaluateExpressions(record_info.extra_log_labels);
      record_info.expressions_evaluated = true;
    }
    fillAuthzDryRunInfo(record_info.extra_log_labels);
    // It's possible that for a short lived TCP connection, we log TCP
    // Connection Open log entry on connection close.
    if (!record_info.tcp_open_entry_logged &&
        request_info.tcp_connection_state == ::Wasm::Common::TCPConnectionState::Close) {
      record_info.request_info->tcp_connection_state = ::Wasm::Common::TCPConnectionState::Open;
      logger_->addTcpLogEntry(*record_info.request_info, peer_node_info.get(),
                              record_info.extra_log_labels, record_info.request_info->start_time,
                              outbound, false /* audit */);
      record_info.request_info->tcp_connection_state = ::Wasm::Common::TCPConnectionState::Close;
    }
    logger_->addTcpLogEntry(request_info, peer_node_info.get(), record_info.extra_log_labels,
                            getCurrentTimeNanoseconds(), outbound, false /* audit */);
  }

  // TODO(dougreid): confirm that audit should override filtering.
  if (enableAuditLog() && shouldAuditThisRequest()) {
    if (!extended_info_populated) {
      ::Wasm::Common::populateExtendedRequestInfo(&request_info);
    }
    // It's possible that for a short lived TCP connection, we audit log TCP
    // Connection Open log entry on connection close.
    if (!record_info.tcp_open_entry_logged &&
        request_info.tcp_connection_state == ::Wasm::Common::TCPConnectionState::Close) {
      record_info.request_info->tcp_connection_state = ::Wasm::Common::TCPConnectionState::Open;
      logger_->addTcpLogEntry(*record_info.request_info, peer_node_info.get(), {},
                              record_info.request_info->start_time, outbound, true /* audit */);
      record_info.request_info->tcp_connection_state = ::Wasm::Common::TCPConnectionState::Close;
    }
    logger_->addTcpLogEntry(*record_info.request_info, peer_node_info.get(), {},
                            record_info.request_info->start_time, outbound, true /* audit */);
  }

  if (log_open_on_timeout) {
    // If we logged the request on timeout, for outbound requests, we try to
    // populate the request info again when metadata is available.
    request_info.is_populated = outbound ? false : true;
  }
  if (!record_info.tcp_open_entry_logged) {
    record_info.tcp_open_entry_logged = true;
  }
  return true;
}

inline bool StackdriverRootContext::isOutbound() {
  return direction_ == ::Wasm::Common::TrafficDirection::Outbound;
}

inline bool StackdriverRootContext::enableAccessLog() {
  return enableAllAccessLog() || enableAccessLogOnError();
}

inline bool StackdriverRootContext::enableAllAccessLog() {
  // TODO(gargnupur): Remove (!config_.disable_server_access_logging() &&
  // !isOutbound) once disable_server_access_logging config is removed.
  return (!config_.disable_server_access_logging() && !isOutbound()) ||
         config_.access_logging() == stackdriver::config::v1alpha1::PluginConfig::FULL;
}

inline bool StackdriverRootContext::evaluateLogFilter() {
  if (config_.access_logging_filter_expression() == "") {
    return true;
  }
  bool value;
  if (!evaluateExpression(log_filter_token_, &value)) {
    LOG_TRACE(absl::StrCat("Could not evaluate expression: ",
                           config_.access_logging_filter_expression()));
    return true;
  }
  return value;
}

inline bool StackdriverRootContext::enableAccessLogOnError() {
  return config_.access_logging() == stackdriver::config::v1alpha1::PluginConfig::ERRORS_ONLY;
}

inline bool StackdriverRootContext::enableAuditLog() { return config_.enable_audit_log(); }

bool StackdriverRootContext::shouldLogThisRequest(::Wasm::Common::RequestInfo& request_info) {
  std::string shouldLog = "";
  if (!getValue({::Wasm::Common::kAccessLogPolicyKey}, &shouldLog)) {
    LOG_DEBUG("cannot get envoy access log info from filter state.");
    return true;
  }
  // Add label log_sampled if Access Log Policy sampling was applied to logs.
  request_info.log_sampled = (shouldLog != "no");
  return request_info.log_sampled;
}

bool StackdriverRootContext::shouldAuditThisRequest() { return Wasm::Common::getAuditPolicy(); }

void StackdriverRootContext::addToTCPRequestQueue(uint32_t id) {
  std::unique_ptr<::Wasm::Common::RequestInfo> request_info =
      std::make_unique<::Wasm::Common::RequestInfo>();
  request_info->tcp_connections_opened++;
  request_info->start_time =
      static_cast<long int>(proxy_wasm::null_plugin::getCurrentTimeNanoseconds());
  std::unique_ptr<StackdriverRootContext::TcpRecordInfo> record_info =
      std::make_unique<StackdriverRootContext::TcpRecordInfo>();
  record_info->request_info = std::move(request_info);
  record_info->tcp_open_entry_logged = false;
  tcp_request_queue_[id] = std::move(record_info);
}

void StackdriverRootContext::deleteFromTCPRequestQueue(uint32_t id) {
  tcp_request_queue_.erase(id);
}

void StackdriverRootContext::incrementReceivedBytes(uint32_t id, size_t size) {
  tcp_request_queue_[id]->request_info->tcp_received_bytes += size;
  tcp_request_queue_[id]->request_info->tcp_total_received_bytes += size;
}

void StackdriverRootContext::incrementSentBytes(uint32_t id, size_t size) {
  tcp_request_queue_[id]->request_info->tcp_sent_bytes += size;
  tcp_request_queue_[id]->request_info->tcp_total_sent_bytes += size;
}

void StackdriverRootContext::incrementConnectionClosed(uint32_t id) {
  tcp_request_queue_[id]->request_info->tcp_connections_closed++;
}

void StackdriverRootContext::setConnectionState(uint32_t id,
                                                ::Wasm::Common::TCPConnectionState state) {
  tcp_request_queue_[id]->request_info->tcp_connection_state = state;
}

void StackdriverRootContext::evaluateExpressions(
    std::unordered_map<std::string, std::string>& extra_labels) {
  for (const auto& expression : expressions_) {
    std::string value;
    if (!evaluateExpression(expression.token, &value)) {
      LOG_TRACE(absl::StrCat("Could not evaluate expression: ", expression.expression));
      continue;
    }
    extra_labels[expression.tag] = value;
  }
}

void StackdriverRootContext::evaluateMetricsExpressions(override_map& overrides) {
  for (const auto& expression : metrics_expressions_) {
    std::string value;
    if (!evaluateExpression(expression.token, &value)) {
      LOG_WARN(absl::StrCat("Could not evaluate expression: ", expression.expression));
      continue;
    }
    overrides[expression.metric].emplace_back(expression.tag, value);
  }
}

void StackdriverRootContext::cleanupExpressions() {
  for (const auto& expression : expressions_) {
    exprDelete(expression.token);
  }
  expressions_.clear();
}

void StackdriverRootContext::cleanupMetricsExpressions() {
  for (const auto& expression : metrics_expressions_) {
    exprDelete(expression.token);
  }
  metrics_expressions_.clear();
}

void StackdriverRootContext::cleanupLogFilter() {
  exprDelete(log_filter_token_);
  log_filter_token_ = 0;
}

// TODO(bianpengyuan) Add final export once root context supports onDone.
// https://github.com/envoyproxy/envoy-wasm/issues/240

StackdriverRootContext* StackdriverContext::getRootContext() {
  RootContext* root = this->root();
  return dynamic_cast<StackdriverRootContext*>(root);
}

void StackdriverContext::onLog() {
  if (!is_initialized_) {
    return;
  }
  if (is_tcp_) {
    getRootContext()->incrementConnectionClosed(context_id_);
    getRootContext()->setConnectionState(context_id_, ::Wasm::Common::TCPConnectionState::Close);
    getRootContext()->recordTCP(context_id_);
    getRootContext()->deleteFromTCPRequestQueue(context_id_);
    return;
  }
  // Record telemetry based on request info.
  getRootContext()->record();
}

} // namespace Stackdriver

#ifdef NULL_PLUGIN
} // namespace null_plugin
} // namespace proxy_wasm
#endif
