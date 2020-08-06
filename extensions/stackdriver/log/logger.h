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

  // Add a new log entry based on the given request information and peer node
  // information.
  void addLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                   const ::Wasm::Common::FlatNode& peer_node_info);

  // Add a new tcp log entry based on the given request information and peer
  // node information.
  void addTcpLogEntry(const ::Wasm::Common::RequestInfo& request_info,
                      const ::Wasm::Common::FlatNode& peer_node_info,
                      long int log_time);

  // Add a new audit entry based on the given request information and peer node
  // information.
  void addAuditEntry(const ::Wasm::Common::RequestInfo& request_info,
                     const ::Wasm::Common::FlatNode& peer_node_info);

  // Add a new tcp audit entry based on the given request information and peer
  // node information.
  void addTcpAuditEntry(const ::Wasm::Common::RequestInfo& request_info,
                        const ::Wasm::Common::FlatNode& peer_node_info,
                        long int log_time);

  // Export and clean the buffered WriteLogEntriesRequests. Returns true if
  // async call is made to export log entry, otherwise returns false if nothing
  // exported.
  bool exportLogEntry(bool is_on_done);

 private:
  // Add a new entry based on the given request information and peer node
  // information, which is either an audit log entry or a regular log entry,
  // depending on the value of 'for_audit'.
  void addEntry(const ::Wasm::Common::RequestInfo& request_info,
                const ::Wasm::Common::FlatNode& peer_node_info, bool for_audit);

  // Add a new tcp entry based on the given request information and peer node
  // information, which is either an audit log entry or a regular log entry,
  // depending on the value of 'for_audit'.
  void addTcpEntry(const ::Wasm::Common::RequestInfo& request_info,
                   const ::Wasm::Common::FlatNode& peer_node_info,
                   long int log_time, bool for_audit);

  // Flush rotates the current WriteLogEntriesRequest. This will be
  // triggered either by a timer or by request size limit. Returns false if
  // there is no log entry to be exported.
  bool flush(bool for_audit);

  // Set labels which don't change between requests.
  void setCommonLabels(const ::Wasm::Common::FlatNode& local_node_info,
                       bool for_audit);

  // Add TCP Specific labels to LogEntry.
  void addTCPLabelsToEntry(const ::Wasm::Common::RequestInfo& request_info,
                              const ::Wasm::Common::FlatNode& peer_node_info,
                              google::logging::v2::LogEntry* log_entry);

  // Fill Http_Request entry in LogEntry.
  void fillHTTPRequestInEntry(const ::Wasm::Common::RequestInfo& request_info,
                              google::logging::v2::LogEntry* log_entry,
                              bool for_audit);

  // Generic method to fill log entry and flush it.
  void fillAndFlushEntry(const ::Wasm::Common::RequestInfo& request_info,
                         const ::Wasm::Common::FlatNode& peer_node_info,
                         google::logging::v2::LogEntry* new_entry,
                         bool for_audit);

  // Buffer for WriteLogEntriesRequests that are to be exported.
  std::vector<
      std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>
      request_queue_;

  // Request that the new log entry should be written into.
  std::unique_ptr<google::logging::v2::WriteLogEntriesRequest>
      log_entries_request_;

  // Audit entry request that audit entries are written into.
  std::unique_ptr<google::logging::v2::WriteLogEntriesRequest>
      audit_entries_request_;

  // Estimated size of the current WriteLogEntriesRequest.
  int size_ = 0;

  // Estimated size of the current audit WriteLogEntriesRequest.
  int audit_size_ = 0;

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
