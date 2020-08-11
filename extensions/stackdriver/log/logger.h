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

#pragma once

#include <string>
#include <vector>

#include "extensions/common/context.h"
#include "extensions/stackdriver/log/exporter.h"
#include "google/logging/v2/logging.pb.h"

namespace Extensions {
namespace Stackdriver {
namespace Log {

#ifdef NULL_PLUGIN
using proxy_wasm::null_plugin::Extensions::Stackdriver::Log::Exporter;
#endif

// Logger records access logs and exports them to Stackdriver.
class Logger {
 public:
  // Logger initiate a Stackdriver access logger, which batches log entries and
  // exports to Stackdriver backend with the given exporter.
  // log_request_size_limit is the size limit of a logging request:
  // https://cloud.google.com/logging/quotas.
  Logger(const ::Wasm::Common::FlatNode& local_node_info,
         std::unique_ptr<Exporter> exporter,
         int log_request_size_limit = 4000000 /* 4 Mb */);

  // Type of log entry.
  enum LogEntryType { ServerAudit, Server };

  // Add a new log entry based on the given request information and peer node
  // information.
  void addLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                   const ::Wasm::Common::FlatNode& peer_node_info,
                   LogEntryType log_type);

  // Add a new tcp log entry based on the given request information and peer
  // node information.
  void addTcpLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                      const ::Wasm::Common::FlatNode& peer_node_info,
                      long int log_time, LogEntryType log_type);

  // Export and clean the buffered WriteLogEntriesRequests. Returns true if
  // async call is made to export log entry, otherwise returns false if nothing
  // exported.
  bool exportLogEntry(bool is_on_done);

 private:
  // Stores log entry request and it's size.
  struct WriteLogEntryRequest {
    // Request that the new log entry should be written into.
    std::unique_ptr<google::logging::v2::WriteLogEntriesRequest> request;
    // Estimated size of the current WriteLogEntriesRequest.
    int size;
  };

  void initializeLogEntryRequest(
      const google::api::MonitoredResource& monitored_resource,
      const ::Wasm::Common::FlatNode& local_node_info, LogEntryType log_type);

  // Flush rotates the current WriteLogEntriesRequest. This will be
  // triggered either by a timer or by request size limit. Returns false if
  // there is no log entry to be exported.
  bool flush(LogEntryType log_type);

  // Set labels which don't change between requests.
  void setCommonLabels(const ::Wasm::Common::FlatNode& local_node_info,
                       LogEntryType log_type);

  // Add TCP Specific labels to LogEntry.
  void addTCPLabelsToLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                              const ::Wasm::Common::FlatNode& peer_node_info,
                              google::logging::v2::LogEntry* log_entry);

  // Fill Http_Request entry in LogEntry.
  void fillHTTPRequestInLogEntry(
      const ::Wasm::Common::RequestInfo& request_info,
      google::logging::v2::LogEntry* log_entry, LogEntryType log_type);

  // Generic method to fill log entry and flush it.
  void fillAndFlushLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                            const ::Wasm::Common::FlatNode& peer_node_info,
                            google::logging::v2::LogEntry* new_entry,
                            LogEntryType log_type);

  // Checsk if the LogEntryType is for audit entries.
  inline bool isAuditEntry(LogEntryType type);

  // Buffer for WriteLogEntriesRequests that are to be exported.
  std::vector<
      std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>
      request_queue_;

  // Map containing the different types of WriteLogEntryRequests
  std::unordered_map<Logger::LogEntryType,
                     std::unique_ptr<Logger::WriteLogEntryRequest>>
      log_entries_request_map_;

  // Size limit of a WriteLogEntriesRequest. If current WriteLogEntriesRequest
  // exceeds this size limit, flush() will be triggered.
  int log_request_size_limit_;

  // Exporter calls Stackdriver services to export access logs.
  std::unique_ptr<Exporter> exporter_;

  // GCP project that this proxy runs with.
  std::string project_id_;
};

}  // namespace Log
}  // namespace Stackdriver
}  // namespace Extensions
