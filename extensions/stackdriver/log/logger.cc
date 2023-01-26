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

#include "extensions/stackdriver/log/logger.h"

#include "absl/strings/match.h"
#include "extensions/common/util.h"
#include "extensions/stackdriver/common/constants.h"
#include "google/logging/v2/log_entry.pb.h"
#include "google/protobuf/util/time_util.h"
#include "re2/re2.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "include/proxy-wasm/null_plugin.h"

#endif

namespace Extensions {
namespace Stackdriver {
namespace Log {
namespace {
// Matches Rbac Access denied string.
// It is of the format:
// "rbac_access_denied_matched_policy[ns[NAMESPACE]-policy[POLICY]-rule[POLICY_INDEX]]"
const RE2
    rbac_denied_match("rbac_access_denied_matched_policy\\[ns\\[(.*)\\]-policy\\[(.*)\\]-rule\\[("
                      ".*)\\]\\]");
constexpr char rbac_denied_match_prefix[] = "rbac_access_denied_matched_policy";
constexpr char kRbacAccessDenied[] = "AuthzDenied";
void setSourceCanonicalService(const ::Wasm::Common::FlatNode& peer_node_info,
                               google::protobuf::Map<std::string, std::string>* label_map) {
  const auto peer_labels = peer_node_info.labels();
  if (peer_labels) {
    auto ics_iter = peer_labels->LookupByKey(Wasm::Common::kCanonicalServiceLabelName.data());
    if (ics_iter) {
      (*label_map)["source_canonical_service"] = flatbuffers::GetString(ics_iter->value());
    }
  }
}

void setDestinationCanonicalService(const ::Wasm::Common::FlatNode& peer_node_info,
                                    google::protobuf::Map<std::string, std::string>* label_map) {
  const auto peer_labels = peer_node_info.labels();
  if (peer_labels) {
    auto ics_iter = peer_labels->LookupByKey(Wasm::Common::kCanonicalServiceLabelName.data());
    if (ics_iter) {
      (*label_map)["destination_canonical_service"] = flatbuffers::GetString(ics_iter->value());
    }
  }
}

// Set monitored resources derived from local node info.
void setMonitoredResource(const ::Wasm::Common::FlatNode& local_node_info,
                          const std::string& resource_type,
                          google::logging::v2::WriteLogEntriesRequest* log_entries_request) {
  google::api::MonitoredResource monitored_resource;
  Common::getMonitoredResource(resource_type, local_node_info, &monitored_resource);
  log_entries_request->mutable_resource()->CopyFrom(monitored_resource);
}

// Helper methods to fill destination Labels. Which labels are filled depends on
// if the entry is audit or not.
void fillDestinationLabels(const ::Wasm::Common::FlatNode& destination_node_info,
                           google::protobuf::Map<std::string, std::string>* label_map, bool audit) {
  (*label_map)["destination_workload"] =
      flatbuffers::GetString(destination_node_info.workload_name());
  (*label_map)["destination_namespace"] =
      flatbuffers::GetString(destination_node_info.namespace_());

  // Don't set if audit request
  if (!audit) {
    (*label_map)["destination_name"] = flatbuffers::GetString(destination_node_info.name());
  }

  // Add destination app and version label if exist.
  const auto local_labels = destination_node_info.labels();
  if (local_labels) {
    auto version_iter = local_labels->LookupByKey("version");
    if (version_iter && !audit) {
      (*label_map)["destination_version"] = flatbuffers::GetString(version_iter->value());
    }
    // App label is used to correlate workload and its logs in UI.
    auto app_iter = local_labels->LookupByKey("app");
    if (app_iter) {
      (*label_map)["destination_app"] = flatbuffers::GetString(app_iter->value());
    }
    if (label_map->find("destination_canonical_service") == label_map->end()) {
      setDestinationCanonicalService(destination_node_info, label_map);
    }
    auto rev_iter =
        local_labels->LookupByKey(Wasm::Common::kCanonicalServiceRevisionLabelName.data());
    if (rev_iter) {
      (*label_map)["destination_canonical_revision"] = flatbuffers::GetString(rev_iter->value());
    }
  }
}

// Helper methods to fill source Labels. The labels filled depends on whether
// the log entry is audit or not.
void fillSourceLabels(const ::Wasm::Common::FlatNode& source_node_info,
                      google::protobuf::Map<std::string, std::string>* label_map, bool audit) {
  if (!audit) {
    (*label_map)["source_name"] = flatbuffers::GetString(source_node_info.name());
  }
  (*label_map)["source_workload"] = flatbuffers::GetString(source_node_info.workload_name());
  (*label_map)["source_namespace"] = flatbuffers::GetString(source_node_info.namespace_());
  // Add destination app and version label if exist.
  const auto local_labels = source_node_info.labels();
  if (local_labels) {
    auto version_iter = local_labels->LookupByKey("version");
    if (version_iter && !audit) {
      (*label_map)["source_version"] = flatbuffers::GetString(version_iter->value());
    }
    // App label is used to correlate workload and its logs in UI.
    auto app_iter = local_labels->LookupByKey("app");
    if (app_iter) {
      (*label_map)["source_app"] = flatbuffers::GetString(app_iter->value());
    }
    if (label_map->find("source_canonical_service") == label_map->end()) {
      setSourceCanonicalService(source_node_info, label_map);
    }
    auto rev_iter =
        local_labels->LookupByKey(Wasm::Common::kCanonicalServiceRevisionLabelName.data());
    if (rev_iter) {
      (*label_map)["source_canonical_revision"] = flatbuffers::GetString(rev_iter->value());
    }
  }
}

void fillExtraLabels(const std::unordered_map<std::string, std::string>& extra_labels,
                     google::protobuf::Map<std::string, std::string>* label_map) {
  for (const auto& extra_label : extra_labels) {
    (*label_map)[extra_label.first] = extra_label.second;
  }
}

bool fillAuthInfo(const std::string& response_details,
                  google::protobuf::Map<std::string, std::string>* label_map) {
  std::string policy_name, policy_namespace, policy_rule_index;
  if (absl::StartsWith(response_details, rbac_denied_match_prefix)) {
    (*label_map)["response_details"] = kRbacAccessDenied;
    if (RE2::PartialMatch(response_details, rbac_denied_match, &policy_namespace, &policy_name,
                          &policy_rule_index)) {
      (*label_map)["policy_name"] = absl::StrCat(policy_namespace, ".", policy_name);
      (*label_map)["policy_rule"] = policy_rule_index;
    }
    return true;
  }
  return false;
}

} // namespace

using google::protobuf::util::TimeUtil;

// Name of the server access log.
constexpr char kServerAccessLogName[] = "server-accesslog-stackdriver";
// Name of the client access log.
constexpr char kClientAccessLogName[] = "client-accesslog-stackdriver";

// Name of the server audit access log.
constexpr char kServerAuditLogName[] = "server-istio-audit-log";
// Name of the client audit access log.
constexpr char kClientAuditLogName[] = "client-istio-audit-log";

void Logger::initializeLogEntryRequest(
    const flatbuffers::Vector<flatbuffers::Offset<Wasm::Common::KeyVal>>* platform_metadata,
    const ::Wasm::Common::FlatNode& local_node_info,
    const std::unordered_map<std::string, std::string>& extra_labels, bool outbound, bool audit) {
  LogEntryType log_entry_type = GetLogEntryType(outbound, audit);
  log_entries_request_map_[log_entry_type]->request =
      std::make_unique<google::logging::v2::WriteLogEntriesRequest>();
  log_entries_request_map_[log_entry_type]->size = 0;
  auto log_entries_request = log_entries_request_map_[log_entry_type]->request.get();
  const std::string& log_name = audit ? (outbound ? kClientAuditLogName : kServerAuditLogName)
                                      : (outbound ? kClientAccessLogName : kServerAccessLogName);

  log_entries_request->set_log_name("projects/" + project_id_ + "/logs/" + log_name);

  std::string resource_type =
      outbound ? Common::kPodMonitoredResource : Common::kContainerMonitoredResource;
  const auto cluster_iter =
      platform_metadata ? platform_metadata->LookupByKey(Common::kGCPClusterNameKey) : nullptr;
  if (!cluster_iter) {
    // if there is no cluster name, then this is not a kubernetes resource

    const auto instance_iter =
        platform_metadata ? platform_metadata->LookupByKey(Common::kGCPGCEInstanceIDKey) : nullptr;
    const auto creator_iter = platform_metadata
                                  ? platform_metadata->LookupByKey(Common::kGCECreatedByKey.data())
                                  : nullptr;

    if (!instance_iter && !creator_iter) {
      resource_type = Common::kGCEInstanceMonitoredResource;
    } else {
      resource_type = Common::kGenericNode;
    }
  }

  setMonitoredResource(local_node_info, resource_type, log_entries_request);
  auto label_map = log_entries_request->mutable_labels();
  if (!audit) {
    (*label_map)["mesh_uid"] = flatbuffers::GetString(local_node_info.mesh_id());
  }

  // Set common labels shared by all client entries or server entries
  outbound ? fillSourceLabels(local_node_info, label_map, audit)
           : fillDestinationLabels(local_node_info, label_map, audit);
  if (!audit) {
    fillExtraLabels(extra_labels, label_map);
  }
}

Logger::Logger(const ::Wasm::Common::FlatNode& local_node_info, std::unique_ptr<Exporter> exporter,
               const std::unordered_map<std::string, std::string>& extra_labels,
               int log_request_size_limit) {
  const auto platform_metadata = local_node_info.platform_metadata();
  const auto project_iter =
      platform_metadata ? platform_metadata->LookupByKey(Common::kGCPProjectKey) : nullptr;
  if (project_iter) {
    project_id_ = flatbuffers::GetString(project_iter->value());
  }

  // Initalize the current WriteLogEntriesRequest for client/server
  log_entries_request_map_[LogEntryType::Client] = std::make_unique<Logger::WriteLogEntryRequest>();
  initializeLogEntryRequest(platform_metadata, local_node_info, extra_labels, true /*outbound */,
                            false /* audit */);
  log_entries_request_map_[Logger::LogEntryType::Server] =
      std::make_unique<Logger::WriteLogEntryRequest>();
  initializeLogEntryRequest(platform_metadata, local_node_info, extra_labels, false /* outbound */,
                            false /* audit */);
  log_entries_request_map_[LogEntryType::ClientAudit] =
      std::make_unique<Logger::WriteLogEntryRequest>();
  initializeLogEntryRequest(platform_metadata, local_node_info, extra_labels, true /*outbound */,
                            true /* audit */);
  log_entries_request_map_[Logger::LogEntryType::ServerAudit] =
      std::make_unique<Logger::WriteLogEntryRequest>();
  initializeLogEntryRequest(platform_metadata, local_node_info, extra_labels, false /* outbound */,
                            true /* audit */);

  log_request_size_limit_ = log_request_size_limit;
  exporter_ = std::move(exporter);
}

void Logger::addTcpLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                            const ::Wasm::Common::FlatNode& peer_node_info,
                            const std::unordered_map<std::string, std::string>& extra_labels,
                            long int log_time, bool outbound, bool audit) {
  // create a new log entry
  auto* log_entries =
      log_entries_request_map_[GetLogEntryType(outbound, audit)]->request->mutable_entries();
  auto* new_entry = log_entries->Add();

  *new_entry->mutable_timestamp() =
      google::protobuf::util::TimeUtil::NanosecondsToTimestamp(log_time);

  addTCPLabelsToLogEntry(request_info, peer_node_info, new_entry, outbound, audit);
  fillAndFlushLogEntry(request_info, peer_node_info, extra_labels, new_entry, outbound, audit);
}

void Logger::addLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                         const ::Wasm::Common::FlatNode& peer_node_info,
                         const std::unordered_map<std::string, std::string>& extra_labels,
                         bool outbound, bool audit) {
  // create a new log entry
  auto* log_entries =
      log_entries_request_map_[GetLogEntryType(outbound, audit)]->request->mutable_entries();
  auto* new_entry = log_entries->Add();

  *new_entry->mutable_timestamp() =
      google::protobuf::util::TimeUtil::NanosecondsToTimestamp(request_info.start_time);
  fillHTTPRequestInLogEntry(request_info, new_entry);
  fillAndFlushLogEntry(request_info, peer_node_info, extra_labels, new_entry, outbound, audit);
}

void Logger::fillAndFlushLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                                  const ::Wasm::Common::FlatNode& peer_node_info,
                                  const std::unordered_map<std::string, std::string>& extra_labels,
                                  google::logging::v2::LogEntry* new_entry, bool outbound,
                                  bool audit) {
  // match logic from stackdriver.cc that determines if error-only logging.
  if (request_info.response_code >= 400 || request_info.response_flag != ::Wasm::Common::NONE) {
    new_entry->set_severity(::google::logging::type::ERROR);
  } else {
    new_entry->set_severity(::google::logging::type::INFO);
  }

  auto label_map = new_entry->mutable_labels();

  if (outbound) {
    fillDestinationLabels(peer_node_info, label_map, audit);
  } else {
    fillSourceLabels(peer_node_info, label_map, audit);
  }

  (*label_map)["destination_service_host"] = request_info.destination_service_host;
  (*label_map)["destination_service_name"] = request_info.destination_service_name;
  (*label_map)["destination_principal"] = request_info.destination_principal;
  (*label_map)["source_principal"] = request_info.source_principal;

  if (!audit) {
    (*label_map)["response_flag"] = request_info.response_flag;
    (*label_map)["service_authentication_policy"] =
        std::string(::Wasm::Common::AuthenticationPolicyString(request_info.service_auth_policy));
    (*label_map)["protocol"] = ::Wasm::Common::ProtocolString(request_info.request_protocol);
    (*label_map)["log_sampled"] = request_info.log_sampled ? "true" : "false";
    (*label_map)["connection_id"] = std::to_string(request_info.connection_id);
    if (!request_info.route_name.empty()) {
      (*label_map)["route_name"] = request_info.route_name;
    }
    if (!request_info.upstream_host.empty()) {
      (*label_map)["upstream_host"] = request_info.upstream_host;
    }
    (*label_map)["upstream_cluster"] = request_info.upstream_cluster;
    if (!request_info.requested_server_name.empty()) {
      (*label_map)["requested_server_name"] = request_info.requested_server_name;
    }
    if (!request_info.x_envoy_original_path.empty()) {
      (*label_map)["x-envoy-original-path"] = request_info.x_envoy_original_path;
    }
    if (!request_info.x_envoy_original_dst_host.empty()) {
      (*label_map)["x-envoy-original-dst-host"] = request_info.x_envoy_original_dst_host;
    }
    if (!request_info.upstream_transport_failure_reason.empty()) {
      (*label_map)["upstream_transport_failure_reason"] =
          request_info.upstream_transport_failure_reason;
    }
    if (!request_info.response_details.empty()) {
      if (!fillAuthInfo(request_info.response_details, label_map)) {
        (*label_map)["response_details"] = request_info.response_details;
      }
    }
  }

  // Insert trace headers, if exist.
  if (request_info.b3_trace_sampled) {
    new_entry->set_trace("projects/" + project_id_ + "/traces/" + request_info.b3_trace_id);
    new_entry->set_span_id(request_info.b3_span_id);
    new_entry->set_trace_sampled(request_info.b3_trace_sampled);
  }

  // This is done just before flushing, so that any customized label entry can
  // override existing ones.
  if (!audit) {
    fillExtraLabels(extra_labels, new_entry->mutable_labels());
  }
  LogEntryType log_entry_type = GetLogEntryType(outbound, audit);
  // Accumulate estimated size of the request. If the current request exceeds
  // the size limit, flush the request out.
  log_entries_request_map_[log_entry_type]->size += new_entry->ByteSizeLong();
  if (log_entries_request_map_[log_entry_type]->size > log_request_size_limit_) {
    flush(log_entry_type);
  }
}

void Logger::flush(LogEntryType log_entry_type) {
  auto request = log_entries_request_map_[log_entry_type]->request.get();
  std::unique_ptr<google::logging::v2::WriteLogEntriesRequest> cur =
      std::make_unique<google::logging::v2::WriteLogEntriesRequest>();
  cur->set_log_name(request->log_name());
  cur->mutable_resource()->CopyFrom(request->resource());
  *cur->mutable_labels() = request->labels();

  // Swap the new request with the old one and export it.
  log_entries_request_map_[log_entry_type]->request.swap(cur);
  request_queue_.emplace_back(std::move(cur));

  // Reset size counter.
  log_entries_request_map_[log_entry_type]->size = 0;
}

bool Logger::flush() {
  bool flushed = false;

  // This flush is triggered by timer, thus iterate through the map to see if
  // any log entry is non empty.
  for (auto const& log_entry : log_entries_request_map_) {
    if (log_entry.second->size != 0) {
      flush(log_entry.first);
      flushed = true;
    }
  }

  return flushed;
}

bool Logger::exportLogEntry(bool is_on_done) {
  if (!flush() && request_queue_.empty()) {
    // No log entry needs to export.
    return false;
  }
  exporter_->exportLogs(request_queue_, is_on_done);
  request_queue_.clear();
  return true;
}

void Logger::addTCPLabelsToLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                                    const ::Wasm::Common::FlatNode& peer_node_info,
                                    google::logging::v2::LogEntry* log_entry, bool outbound,
                                    bool audit) {
  const auto& entries_request = log_entries_request_map_[GetLogEntryType(outbound, audit)]->request;
  auto label_map = log_entry->mutable_labels();
  std::string source, destination;
  if (outbound) {
    setDestinationCanonicalService(peer_node_info, label_map);
    auto source_cs_iter = entries_request->labels().find("source_canonical_service");
    auto destination_cs_iter = label_map->find("destination_canonical_service");
    source = source_cs_iter != entries_request->labels().end()
                 ? source_cs_iter->second
                 : entries_request->labels().at("source_workload");
    destination = destination_cs_iter != label_map->end() ? destination_cs_iter->second
                                                          : request_info.destination_service_name;
  } else {
    setSourceCanonicalService(peer_node_info, label_map);
    auto source_cs_iter = label_map->find("source_canonical_service");
    auto destination_cs_iter = entries_request->labels().find("destination_canonical_service");
    source = source_cs_iter != label_map->end()
                 ? source_cs_iter->second
                 : flatbuffers::GetString(peer_node_info.workload_name());
    destination = destination_cs_iter != entries_request->labels().end()
                      ? destination_cs_iter->second
                      : request_info.destination_service_name;
  }
  log_entry->set_text_payload(absl::StrCat(source, " --> ", destination));
  (*label_map)["source_ip"] = request_info.source_address;
  (*label_map)["destination_ip"] = request_info.destination_address;
  (*label_map)["source_port"] = std::to_string(request_info.source_port);
  (*label_map)["destination_port"] = std::to_string(request_info.destination_port);
  (*label_map)["total_sent_bytes"] = std::to_string(request_info.tcp_total_sent_bytes);
  (*label_map)["total_received_bytes"] = std::to_string(request_info.tcp_total_received_bytes);
  (*label_map)["connection_state"] =
      std::string(::Wasm::Common::TCPConnectionStateString(request_info.tcp_connection_state));
}

void Logger::fillHTTPRequestInLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                                       google::logging::v2::LogEntry* log_entry) {
  auto http_request = log_entry->mutable_http_request();
  http_request->set_request_method(request_info.request_operation);
  http_request->set_request_url(request_info.url_scheme + "://" + request_info.url_host +
                                request_info.path);
  http_request->set_request_size(request_info.request_size);
  http_request->set_status(request_info.response_code);
  http_request->set_response_size(request_info.response_size);
  http_request->set_user_agent(request_info.user_agent);
  http_request->set_remote_ip(request_info.source_address);
  http_request->set_server_ip(request_info.destination_address);
  http_request->set_protocol(::Wasm::Common::ProtocolString(request_info.request_protocol).data());
  *http_request->mutable_latency() =
      google::protobuf::util::TimeUtil::NanosecondsToDuration(request_info.duration);
  http_request->set_referer(request_info.referer);
  auto label_map = log_entry->mutable_labels();
  (*label_map)["request_id"] = request_info.request_id;
}

} // namespace Log
} // namespace Stackdriver
} // namespace Extensions
