/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include <map>
#include <string>

#include "envoy/http/header_map.h"
#include "envoy/network/connection.h"
#include "google/protobuf/util/json_util.h"

namespace Envoy {
namespace Utils {

// Extract HTTP headers into a string map
void ExtractHeaders(const Http::HeaderMap& header_map, const std::set<std::string>& exclusives,
                    std::map<std::string, std::string>& headers);

// Find the given headers from the header map and extract them out to the string
// map.
void FindHeaders(const Http::HeaderMap& header_map, const std::set<std::string>& inclusives,
                 std::map<std::string, std::string>& headers);

// Get ip and port from Envoy ip.
bool GetIpPort(const Network::Address::Ip* ip, std::string* str_ip, int* port);

// Get destination.uid attribute value from metadata.
bool GetDestinationUID(const envoy::config::core::v3::Metadata& metadata, std::string* uid);

// Get peer or local principal URI.
bool GetPrincipal(const Network::Connection* connection, bool peer, std::string* principal);

// Get peer or local trust domain.
bool GetTrustDomain(const Network::Connection* connection, bool peer, std::string* trust_domain);

// Returns true if connection is mutual TLS enabled.
bool IsMutualTLS(const Network::Connection* connection);

// Get requested server name, SNI in case of TLS
bool GetRequestedServerName(const Network::Connection* connection, std::string* name);

// Parse JSON string into message.
absl::Status ParseJsonMessage(const std::string& json, ::google::protobuf::Message* output);

// Get the namespace part of Istio certificate URI.
absl::optional<absl::string_view> GetNamespace(absl::string_view principal);

} // namespace Utils
} // namespace Envoy
