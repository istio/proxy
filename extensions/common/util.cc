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

#include <string>

namespace Wasm {
namespace Common {

namespace {

// This replicates the flag lists in envoyproxy/envoy, because the property
// access API does not support returning response flags as a short string since
// it is not owned by any object and always generated on demand:
// https://github.com/envoyproxy/envoy/blob/v1.12.0/source/common/stream_info/utility.cc#L8
const std::string NONE = "-";
const std::string DOWNSTREAM_CONNECTION_TERMINATION = "DC";
const std::string FAILED_LOCAL_HEALTH_CHECK = "LH";
const std::string NO_HEALTHY_UPSTREAM = "UH";
const std::string UPSTREAM_REQUEST_TIMEOUT = "UT";
const std::string LOCAL_RESET = "LR";
const std::string UPSTREAM_REMOTE_RESET = "UR";
const std::string UPSTREAM_CONNECTION_FAILURE = "UF";
const std::string UPSTREAM_CONNECTION_TERMINATION = "UC";
const std::string UPSTREAM_OVERFLOW = "UO";
const std::string UPSTREAM_RETRY_LIMIT_EXCEEDED = "URX";
const std::string NO_ROUTE_FOUND = "NR";
const std::string DELAY_INJECTED = "DI";
const std::string FAULT_INJECTED = "FI";
const std::string RATE_LIMITED = "RL";
const std::string UNAUTHORIZED_EXTERNAL_SERVICE = "UAEX";
const std::string RATELIMIT_SERVICE_ERROR = "RLSE";
const std::string STREAM_IDLE_TIMEOUT = "SI";
const std::string INVALID_ENVOY_REQUEST_HEADERS = "IH";
const std::string DOWNSTREAM_PROTOCOL_ERROR = "DPE";

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
  LastFlag = DownstreamProtocolError
};

void appendString(std::string& result, const std::string& append) {
  if (result.empty()) {
    result = append;
  } else {
    result += "," + append;
  }
}

}  // namespace

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

  if (response_flag >= (LastFlag << 1)) {
    // Response flag integer overflows. Append the integer to avoid information
    // loss.
    appendString(result, std::to_string(response_flag));
  }

  return result.empty() ? NONE : result;
}

}  // namespace Common
}  // namespace Wasm
