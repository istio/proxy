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
#include "extensions/stackdriver/common/utils.h"
#include "google/logging/v2/log_entry.pb.h"
#include "google/protobuf/util/time_util.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "extensions/common/wasm/null/null_plugin.h"

#endif

namespace Extensions {
namespace Stackdriver {
namespace Log {

using google::protobuf::util::TimeUtil;

// Name of the HTTP server access log.
constexpr char kServerAccessLogName[] = "server-accesslog-stackdriver";

Logger::Logger(const ::wasm::common::NodeInfo& local_node_info,
               std::unique_ptr<Exporter> exporter, int log_request_size_limit) {
  // Initalize the current WriteLogEntriesRequest.
  log_entries_request_ =
      std::make_unique<google::logging::v2::WriteLogEntriesRequest>();

  // Set log names.
  const auto& platform_metadata = local_node_info.platform_metadata();
  const auto project_iter = platform_metadata.find(Common::kGCPProjectKey);
  std::string project_id = "";
  if (project_iter != platform_metadata.end()) {
    project_id = project_iter->second;
  }
  log_entries_request_->set_log_name("projects/" + project_id + "/logs/" +
                                     kServerAccessLogName);

  std::string resource_type = Common::kContainerMonitoredResource;
  const auto cluster_iter = platform_metadata.find(Common::kGCPClusterNameKey);
  if (platform_metadata.end() == cluster_iter) {
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
  (*label_map)["destination_name"] = local_node_info.name();
  (*label_map)["destination_workload"] = local_node_info.workload_name();
  (*label_map)["destination_namespace"] = local_node_info.namespace_();
  (*label_map)["mesh_uid"] = local_node_info.mesh_id();
  log_request_size_limit_ = log_request_size_limit;
  exporter_ = std::move(exporter);
}

void Logger::addLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                         const ::wasm::common::NodeInfo& peer_node_info) {
  // create a new log entry
  auto* log_entries = log_entries_request_->mutable_entries();
  auto* new_entry = log_entries->Add();

  *new_entry->mutable_timestamp() =
      TimeUtil::NanosecondsToTimestamp(request_info.start_timestamp);
  new_entry->set_severity(::google::logging::type::INFO);
  auto label_map = new_entry->mutable_labels();
  (*label_map)["source_name"] = peer_node_info.name();
  (*label_map)["source_workload"] = peer_node_info.workload_name();
  (*label_map)["source_namespace"] = peer_node_info.namespace_();

  (*label_map)["request_operation"] = request_info.request_operation;
  (*label_map)["destination_service_host"] =
      request_info.destination_service_host;
  (*label_map)["response_flag"] = request_info.response_flag;
  (*label_map)["protocol"] = request_info.request_protocol;
  (*label_map)["destination_principal"] = request_info.destination_principal;
  (*label_map)["source_principal"] = request_info.source_principal;
  (*label_map)["service_authentication_policy"] =
      std::string(::Wasm::Common::AuthenticationPolicyString(
          request_info.service_auth_policy));
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

void Logger::exportLogEntry() {
  if (!flush() && request_queue_.empty()) {
    // No log entry needs to export.
    return;
  }
  exporter_->exportLogs(request_queue_);
  request_queue_.clear();
}

}  // namespace Log
}  // namespace Stackdriver
}  // namespace Extensions
