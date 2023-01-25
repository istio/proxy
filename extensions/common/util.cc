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

#include "extensions/common/util.h"

#include "absl/strings/str_cat.h"

namespace Wasm {
namespace Common {

namespace {

// This replicates the flag lists in envoyproxy/envoy, because the property
// access API does not support returning response flags as a short string since
// it is not owned by any object and always generated on demand:
// https://github.com/envoyproxy/envoy/blob/v1.18.3/source/common/stream_info/utility.h#L21
constexpr static absl::string_view DOWNSTREAM_CONNECTION_TERMINATION = "DC";
constexpr static absl::string_view FAILED_LOCAL_HEALTH_CHECK = "LH";
constexpr static absl::string_view NO_HEALTHY_UPSTREAM = "UH";
constexpr static absl::string_view UPSTREAM_REQUEST_TIMEOUT = "UT";
constexpr static absl::string_view LOCAL_RESET = "LR";
constexpr static absl::string_view UPSTREAM_REMOTE_RESET = "UR";
constexpr static absl::string_view UPSTREAM_CONNECTION_FAILURE = "UF";
constexpr static absl::string_view UPSTREAM_CONNECTION_TERMINATION = "UC";
constexpr static absl::string_view UPSTREAM_OVERFLOW = "UO";
constexpr static absl::string_view UPSTREAM_RETRY_LIMIT_EXCEEDED = "URX";
constexpr static absl::string_view NO_ROUTE_FOUND = "NR";
constexpr static absl::string_view DELAY_INJECTED = "DI";
constexpr static absl::string_view FAULT_INJECTED = "FI";
constexpr static absl::string_view RATE_LIMITED = "RL";
constexpr static absl::string_view UNAUTHORIZED_EXTERNAL_SERVICE = "UAEX";
constexpr static absl::string_view RATELIMIT_SERVICE_ERROR = "RLSE";
constexpr static absl::string_view STREAM_IDLE_TIMEOUT = "SI";
constexpr static absl::string_view INVALID_ENVOY_REQUEST_HEADERS = "IH";
constexpr static absl::string_view DOWNSTREAM_PROTOCOL_ERROR = "DPE";
constexpr static absl::string_view UPSTREAM_MAX_STREAM_DURATION_REACHED = "UMSDR";
constexpr static absl::string_view RESPONSE_FROM_CACHE_FILTER = "RFCF";
constexpr static absl::string_view NO_FILTER_CONFIG_FOUND = "NFCF";
constexpr static absl::string_view DURATION_TIMEOUT = "DT";
constexpr static absl::string_view UPSTREAM_PROTOCOL_ERROR = "UPE";
constexpr static absl::string_view NO_CLUSTER_FOUND = "NC";
constexpr static absl::string_view OVERLOAD_MANAGER = "OM";
constexpr static absl::string_view DNS_RESOLUTION_FAILURE = "DF";

enum ResponseFlag {
  FailedLocalHealthCheck = 0x1,
  NoHealthyUpstream = 0x2,
  UpstreamRequestTimeout = 0x4,
  LocalReset = 0x8,
  UpstreamRemoteReset = 0x10,
  UpstreamConnectionFailure = 0x20,
  UpstreamConnectionTermination = 0x40,
  UpstreamOverflow = 0x80,
  NoRouteFound = 0x100,
  DelayInjected = 0x200,
  FaultInjected = 0x400,
  RateLimited = 0x800,
  UnauthorizedExternalService = 0x1000,
  RateLimitServiceError = 0x2000,
  DownstreamConnectionTermination = 0x4000,
  UpstreamRetryLimitExceeded = 0x8000,
  StreamIdleTimeout = 0x10000,
  InvalidEnvoyRequestHeaders = 0x20000,
  DownstreamProtocolError = 0x40000,
  UpstreamMaxStreamDurationReached = 0x80000,
  ResponseFromCacheFilter = 0x100000,
  NoFilterConfigFound = 0x200000,
  DurationTimeout = 0x400000,
  UpstreamProtocolError = 0x800000,
  NoClusterFound = 0x1000000,
  OverloadManager = 0x2000000,
  DnsResolutionFailed = 0x4000000,
  LastFlag = DnsResolutionFailed,
};

void appendString(std::string& result, const absl::string_view& append) {
  if (result.empty()) {
    result = std::string(append);
  } else {
    absl::StrAppend(&result, ",", append);
  }
}

} // namespace

const std::string parseResponseFlag(uint64_t response_flag) {
  std::string result;

  if (response_flag & FailedLocalHealthCheck) {
    appendString(result, FAILED_LOCAL_HEALTH_CHECK);
  }

  if (response_flag & NoHealthyUpstream) {
    appendString(result, NO_HEALTHY_UPSTREAM);
  }

  if (response_flag & UpstreamRequestTimeout) {
    appendString(result, UPSTREAM_REQUEST_TIMEOUT);
  }

  if (response_flag & LocalReset) {
    appendString(result, LOCAL_RESET);
  }

  if (response_flag & UpstreamRemoteReset) {
    appendString(result, UPSTREAM_REMOTE_RESET);
  }

  if (response_flag & UpstreamConnectionFailure) {
    appendString(result, UPSTREAM_CONNECTION_FAILURE);
  }

  if (response_flag & UpstreamConnectionTermination) {
    appendString(result, UPSTREAM_CONNECTION_TERMINATION);
  }

  if (response_flag & UpstreamOverflow) {
    appendString(result, UPSTREAM_OVERFLOW);
  }

  if (response_flag & NoRouteFound) {
    appendString(result, NO_ROUTE_FOUND);
  }

  if (response_flag & DelayInjected) {
    appendString(result, DELAY_INJECTED);
  }

  if (response_flag & FaultInjected) {
    appendString(result, FAULT_INJECTED);
  }

  if (response_flag & RateLimited) {
    appendString(result, RATE_LIMITED);
  }

  if (response_flag & UnauthorizedExternalService) {
    appendString(result, UNAUTHORIZED_EXTERNAL_SERVICE);
  }

  if (response_flag & RateLimitServiceError) {
    appendString(result, RATELIMIT_SERVICE_ERROR);
  }

  if (response_flag & DownstreamConnectionTermination) {
    appendString(result, DOWNSTREAM_CONNECTION_TERMINATION);
  }

  if (response_flag & UpstreamRetryLimitExceeded) {
    appendString(result, UPSTREAM_RETRY_LIMIT_EXCEEDED);
  }

  if (response_flag & StreamIdleTimeout) {
    appendString(result, STREAM_IDLE_TIMEOUT);
  }

  if (response_flag & InvalidEnvoyRequestHeaders) {
    appendString(result, INVALID_ENVOY_REQUEST_HEADERS);
  }

  if (response_flag & DownstreamProtocolError) {
    appendString(result, DOWNSTREAM_PROTOCOL_ERROR);
  }

  if (response_flag & UpstreamMaxStreamDurationReached) {
    appendString(result, UPSTREAM_MAX_STREAM_DURATION_REACHED);
  }

  if (response_flag & ResponseFromCacheFilter) {
    appendString(result, RESPONSE_FROM_CACHE_FILTER);
  }

  if (response_flag & NoFilterConfigFound) {
    appendString(result, NO_FILTER_CONFIG_FOUND);
  }

  if (response_flag & DurationTimeout) {
    appendString(result, DURATION_TIMEOUT);
  }

  if (response_flag & UpstreamProtocolError) {
    appendString(result, UPSTREAM_PROTOCOL_ERROR);
  }

  if (response_flag & NoClusterFound) {
    appendString(result, NO_CLUSTER_FOUND);
  }

  if (response_flag & OverloadManager) {
    appendString(result, OVERLOAD_MANAGER);
  }

  if (response_flag & DnsResolutionFailed) {
    appendString(result, DNS_RESOLUTION_FAILURE);
  }

  if (response_flag >= (LastFlag << 1)) {
    // Response flag integer overflows. Append the integer to avoid information
    // loss.
    appendString(result, std::to_string(response_flag));
  }

  return result.empty() ? ::Wasm::Common::NONE : result;
}

} // namespace Common
} // namespace Wasm
