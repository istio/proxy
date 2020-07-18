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

#include "extensions/stackdriver/common/constants.h"
#include "google/logging/v2/log_entry.pb.h"
#include "google/protobuf/util/time_util.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "include/proxy-wasm/null_plugin.h"

#endif

namespace Extensions {
namespace Stackdriver {
namespace Log {

using google::protobuf::util::TimeUtil;

// Name of the HTTP server access log.
constexpr char kServerAccessLogName[] = "server-accesslog-stackdriver";

Logger::Logger(const ::Wasm::Common::FlatNode& local_node_info,
               std::unique_ptr<Exporter> exporter, int log_request_size_limit) {
  // Initalize the current WriteLogEntriesRequest.
  log_entries_request_ =
      std::make_unique<google::logging::v2::WriteLogEntriesRequest>();

  // Set log names.
  const auto platform_metadata = local_node_info.platform_metadata();
  const auto project_iter =
      platform_metadata ? platform_metadata->LookupByKey(Common::kGCPProjectKey)
                        : nullptr;
  if (project_iter) {
    project_id_ = flatbuffers::GetString(project_iter->value());
  }
  log_entries_request_->set_log_name("projects/" + project_id_ + "/logs/" +
                                     kServerAccessLogName);

  std::string resource_type = Common::kContainerMonitoredResource;
  const auto cluster_iter =
      platform_metadata
          ? platform_metadata->LookupByKey(Common::kGCPClusterNameKey)
          : nullptr;
  if (!cluster_iter) {
    // if there is no cluster name, then this is a gce_instance
    resource_type = Common::kGCEInstanceMonitoredResource;
  }

  // Set monitored resources derived from local node info.
  google::api::MonitoredResource monitored_resource;
  Common::getMonitoredResource(resource_type, local_node_info,
                               &monitored_resource);
  log_entries_request_->mutable_resource()->CopyFrom(monitored_resource);

  // Set common labels shared by all entries.
  auto label_map = log_entries_request_->mutable_labels();
  (*label_map)["destination_name"] =
      flatbuffers::GetString(local_node_info.name());
  (*label_map)["destination_workload"] =
      flatbuffers::GetString(local_node_info.workload_name());
  (*label_map)["destination_namespace"] =
      flatbuffers::GetString(local_node_info.namespace_());
  (*label_map)["mesh_uid"] = flatbuffers::GetString(local_node_info.mesh_id());
  // Add destination app and version label if exist.
  const auto local_labels = local_node_info.labels();
  if (local_labels) {
    auto version_iter = local_labels->LookupByKey("version");
    if (version_iter) {
      (*label_map)["destination_version"] =
          flatbuffers::GetString(version_iter->value());
    }
    // App label is used to correlate workload and its logs in UI.
    auto app_iter = local_labels->LookupByKey("app");
    if (app_iter) {
      (*label_map)["destination_app"] =
          flatbuffers::GetString(app_iter->value());
    }
    auto ics_iter = local_labels->LookupByKey(
        Wasm::Common::kCanonicalServiceLabelName.data());
    if (ics_iter) {
      (*label_map)["destination_canonical_service"] =
          flatbuffers::GetString(ics_iter->value());
    }
    auto rev_iter = local_labels->LookupByKey(
        Wasm::Common::kCanonicalServiceRevisionLabelName.data());
    if (rev_iter) {
      (*label_map)["destination_canonical_revision"] =
          flatbuffers::GetString(rev_iter->value());
    }
  }
  log_request_size_limit_ = log_request_size_limit;
  exporter_ = std::move(exporter);
}

void Logger::addLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                         const ::Wasm::Common::FlatNode& peer_node_info,
                         bool is_tcp) {
  // create a new log entry
  auto* log_entries = log_entries_request_->mutable_entries();
  auto* new_entry = log_entries->Add();

  *new_entry->mutable_timestamp() =
      google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
          request_info.start_time);
  new_entry->set_severity(::google::logging::type::INFO);
  auto label_map = new_entry->mutable_labels();
  (*label_map)["request_id"] = request_info.request_id;
  (*label_map)["source_name"] = flatbuffers::GetString(peer_node_info.name());
  (*label_map)["source_workload"] =
      flatbuffers::GetString(peer_node_info.workload_name());
  (*label_map)["source_namespace"] =
      flatbuffers::GetString(peer_node_info.namespace_());
  // Add source app and version label if exist.
  const auto peer_labels = peer_node_info.labels();
  if (peer_labels) {
    auto version_iter = peer_labels->LookupByKey("version");
    if (version_iter) {
      (*label_map)["source_version"] =
          flatbuffers::GetString(version_iter->value());
    }
    auto app_iter = peer_labels->LookupByKey("app");
    if (app_iter) {
      (*label_map)["source_app"] = flatbuffers::GetString(app_iter->value());
    }
    auto ics_iter = peer_labels->LookupByKey(
        Wasm::Common::kCanonicalServiceLabelName.data());
    if (ics_iter) {
      (*label_map)["source_canonical_service"] =
          flatbuffers::GetString(ics_iter->value());
    }
    auto rev_iter = peer_labels->LookupByKey(
        Wasm::Common::kCanonicalServiceRevisionLabelName.data());
    if (rev_iter) {
      (*label_map)["source_canonical_revision"] =
          flatbuffers::GetString(rev_iter->value());
    }
  }

  (*label_map)["destination_service_host"] =
      request_info.destination_service_host;
  (*label_map)["response_flag"] = request_info.response_flag;
  (*label_map)["destination_principal"] = request_info.destination_principal;
  (*label_map)["source_principal"] = request_info.source_principal;
  (*label_map)["service_authentication_policy"] =
      std::string(::Wasm::Common::AuthenticationPolicyString(
          request_info.service_auth_policy));
  (*label_map)["protocol"] = request_info.request_protocol;
  (*label_map)["log_sampled"] = request_info.log_sampled ? "true" : "false";

  if (is_tcp) {
    addTCPLabelsToLogEntry(request_info, new_entry);
  } else {
    // Insert HTTPRequest
    fillHTTPRequestInLogEntry(request_info, new_entry);
  }
  // Insert trace headers, if exist.
  if (request_info.b3_trace_sampled) {
    new_entry->set_trace("projects/" + project_id_ + "/traces/" +
                         request_info.b3_trace_id);
    new_entry->set_span_id(request_info.b3_span_id);
    new_entry->set_trace_sampled(request_info.b3_trace_sampled);
  }

  // Accumulate estimated size of the request. If the current request exceeds
  // the size limit, flush the request out.
  size_ += new_entry->ByteSizeLong();
  if (size_ > log_request_size_limit_) {
    flush();
  }
}

bool Logger::flush() {
  if (size_ == 0) {
    // This flush is triggered by timer and does not have any log entries.
    return false;
  }

  // Reconstruct a new WriteLogRequest.
  std::unique_ptr<google::logging::v2::WriteLogEntriesRequest> cur =
      std::make_unique<google::logging::v2::WriteLogEntriesRequest>();
  cur->set_log_name(log_entries_request_->log_name());
  cur->mutable_resource()->CopyFrom(log_entries_request_->resource());
  *cur->mutable_labels() = log_entries_request_->labels();

  // Swap the new request with the old one and export it.
  log_entries_request_.swap(cur);
  request_queue_.emplace_back(std::move(cur));

  // Reset size counter.
  size_ = 0;
  return true;
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

void Logger::addTCPLabelsToLogEntry(
    const ::Wasm::Common::RequestInfo& request_info,
    google::logging::v2::LogEntry* log_entry) {
  auto label_map = log_entry->mutable_labels();
  (*label_map)["source_ip"] = request_info.source_address;
  (*label_map)["destination_ip"] = request_info.destination_address;
  (*label_map)["source_port"] = request_info.source_port;
  (*label_map)["destination_port"] = request_info.destination_port;
  (*label_map)["total_sent_bytes"] = request_info.tcp_total_sent_bytes;
  (*label_map)["total_received_bytes"] = request_info.tcp_total_received_bytes;
  (*label_map)["connection_state"] =
      std::string(::Wasm::Common::TCPConnectionStateString(
          request_info.tcp_connection_state));
}

void Logger::fillHTTPRequestInLogEntry(
    const ::Wasm::Common::RequestInfo& request_info,
    google::logging::v2::LogEntry* log_entry) {
  auto http_request = log_entry->mutable_http_request();
  http_request->set_request_method(request_info.request_operation);
  http_request->set_request_url(request_info.url_scheme + "://" +
                                request_info.url_host + request_info.url_path);
  http_request->set_request_size(request_info.request_size);
  http_request->set_status(request_info.response_code);
  http_request->set_response_size(request_info.response_size);
  http_request->set_user_agent(request_info.user_agent);
  http_request->set_remote_ip(request_info.source_address);
  http_request->set_server_ip(request_info.destination_address);
  http_request->set_protocol(request_info.request_protocol);
  *http_request->mutable_latency() =
      google::protobuf::util::TimeUtil::NanosecondsToDuration(
          request_info.duration);
  http_request->set_referer(request_info.referer);
}

}  // namespace Log
}  // namespace Stackdriver
}  // namespace Extensions
