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
  Logger(const ::Wasm::Common::FlatNode& local_node_info, std::unique_ptr<Exporter> exporter,
         const std::unordered_map<std::string, std::string>& extra_labels,
         int log_request_size_limit = 4000000 /* 4 Mb */);

  // Type of log entry.
  enum LogEntryType { Client, ClientAudit, Server, ServerAudit };

  // Add a new log entry based on the given request information and peer node
  // information. The type of entry that is added depends on outbound and audit
  // arguments.
  //
  // Audit labels:
  // - destination_canonical_revision
  // - destination_canonical_service
  // - destination_service_name
  // - destination_namespace
  // - destination_principal
  // - destination_service_host
  // - destination_app
  // - destination_workload
  // - request_id
  // - source_app
  // - source_canonical_revision
  // - source_canonical_service
  // - source_namespace
  // - source_workload
  // - source_principal
  //
  void addLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                   const ::Wasm::Common::FlatNode& peer_node_info,
                   const std::unordered_map<std::string, std::string>& extra_labels, bool outbound,
                   bool audit);

  // Add a new tcp log entry based on the given request information and peer
  // node information. The type of entry that is added depends on outbound and
  // audit arguments.
  void addTcpLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                      const ::Wasm::Common::FlatNode& peer_node_info,
                      const std::unordered_map<std::string, std::string>& extra_labels,
                      long int log_time, bool outbound, bool audit);

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

  // Flush rotates the current WriteLogEntriesRequest. This will be triggered
  // either by a timer or by request size limit. Returns false if there is no
  // log entry to be exported.
  bool flush();
  void flush(LogEntryType log_entry_type);

  // Add TCP Specific labels to LogEntry. Which labels are set depends on if
  // the entry is an audit entry or not
  void addTCPLabelsToLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                              const ::Wasm::Common::FlatNode& peer_node_info,
                              google::logging::v2::LogEntry* log_entry, bool outbound, bool audit);

  // Fill Http_Request entry in LogEntry.
  void fillHTTPRequestInLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                                 google::logging::v2::LogEntry* log_entry);

  // Generic method to fill the log entry. The WriteLogEntriesRequest
  // containing the log entry is flushed if the request exceeds the configured
  // maximum size. Which request should be flushed is determined by the outbound
  // and audit arguments.
  void fillAndFlushLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                            const ::Wasm::Common::FlatNode& peer_node_info,
                            const std::unordered_map<std::string, std::string>& extra_labels,
                            google::logging::v2::LogEntry* new_entry, bool outbound, bool audit);

  // Helper method to initialize log entry request. The type of log entry is
  // determined by the oubound and audit arguments.
  void initializeLogEntryRequest(
      const flatbuffers::Vector<flatbuffers::Offset<Wasm::Common::KeyVal>>* platform_metadata,
      const ::Wasm::Common::FlatNode& local_node_info,
      const std::unordered_map<std::string, std::string>& extra_labels, bool outbound, bool audit);

  // Helper method to get Log Entry Type.
  Logger::LogEntryType GetLogEntryType(bool outbound, bool audit) const {
    if (outbound) {
      if (audit) {
        return Logger::LogEntryType::ClientAudit;
      }
      return Logger::LogEntryType::Client;
    }

    if (audit) {
      return Logger::LogEntryType::ServerAudit;
    }

    return Logger::LogEntryType::Server;
  }

  // Buffer for WriteLogEntriesRequests that are to be exported.
  std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>> request_queue_;

  // Stores client/server requests that the new log entry should be written
  // into.
  std::unordered_map<Logger::LogEntryType, std::unique_ptr<Logger::WriteLogEntryRequest>>
      log_entries_request_map_;

  // Size limit of a WriteLogEntriesRequest. If current WriteLogEntriesRequest
  // exceeds this size limit, flush() will be triggered.
  int log_request_size_limit_;

  // Exporter calls Stackdriver services to export access logs.
  std::unique_ptr<Exporter> exporter_;

  // GCP project that this proxy runs with.
  std::string project_id_;
};

} // namespace Log
} // namespace Stackdriver
} // namespace Extensions
