/*
 * Copyright 2016-2019 Envoy Project Authors
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Common enumerations available to WASM modules and shared with sandbox.
 */
// NOLINT(namespace-envoy)
#pragma once

#include <cstdint>
#include <string>

// Return status of a Proxy-Wasm hostcall. Corresponds to Proxy-Wasm ABI
// `proxy_status_t`.
enum class WasmResult : uint32_t {
  Ok = 0,
  // The result could not be found, e.g. a provided key did not appear in a
  // table.
  NotFound = 1,
  // An argument was bad, e.g. did not not conform to the required range.
  BadArgument = 2,
  // A protobuf could not be serialized.
  SerializationFailure = 3,
  // A protobuf could not be parsed.
  ParseFailure = 4,
  // A provided expression (e.g. "foo.bar") was illegal or unrecognized.
  BadExpression = 5,
  // A provided memory range was not legal.
  InvalidMemoryAccess = 6,
  // Data was requested from an empty container.
  Empty = 7,
  // The provided CAS did not match that of the stored data.
  CasMismatch = 8,
  // Returned result was unexpected, e.g. of the incorrect size.
  ResultMismatch = 9,
  // Internal failure: trying check logs of the surrounding system.
  InternalFailure = 10,
  // The connection/stream/pipe was broken/closed unexpectedly.
  BrokenConnection = 11,
  // Feature not implemented.
  Unimplemented = 12,
};

#define _CASE(_e)                                                                                  \
  case WasmResult::_e:                                                                             \
    return #_e
inline std::string toString(WasmResult r) {
  switch (r) {
    _CASE(Ok);
    _CASE(NotFound);
    _CASE(BadArgument);
    _CASE(SerializationFailure);
    _CASE(ParseFailure);
    _CASE(BadExpression);
    _CASE(InvalidMemoryAccess);
    _CASE(Empty);
    _CASE(CasMismatch);
    _CASE(ResultMismatch);
    _CASE(InternalFailure);
    _CASE(BrokenConnection);
    _CASE(Unimplemented);
  }
  return "Unknown";
}
#undef _CASE

// HTTP header type. Corresponds to Proxy-Wasm ABI `proxy_header_map_type_t`.
enum class WasmHeaderMapType : int32_t {
  // HTTP request headers. During the `onLog` callback these are immutable.
  RequestHeaders = 0,
  // HTTP request trailers. During the `onLog` callback these are immutable.
  RequestTrailers = 1,
  // HTTP response headers. During the `onLog` callback these are immutable.
  ResponseHeaders = 2,
  // HTTP response trailers. During the `onLog` callback these are immutable.
  ResponseTrailers = 3,
  // Initial metadata for the response to a gRPC callout. Immutable.
  GrpcReceiveInitialMetadata = 4,
  // Trailing metadata for the response to a gRPC callout. Immutable.
  GrpcReceiveTrailingMetadata = 5,
  // HTTP response headers for the response to an HTTP callout. Immutable.
  HttpCallResponseHeaders = 6,
  // HTTP response trailers for the response to an HTTP callout. Immutable.
  HttpCallResponseTrailers = 7,
  MAX = 7,
};

// Data buffer types. Corresponds to Proxy-Wasm ABI `proxy_buffer_type_t`.
enum class WasmBufferType : int32_t {
  // HTTP request body bytes. During the `onLog` callback these are immutable.
  HttpRequestBody = 0,
  // HTTP response body bytes. During the `onLog` callback these are immutable.
  HttpResponseBody = 1,
  // Bytes received from downstream TCP (or TCP-like) sender. During the `onLog`
  // callback these are immutable.
  NetworkDownstreamData = 2,
  // Bytes received from upstream TCP (or TCP-like) sender. During the `onLog`
  // callback these are immutable.
  NetworkUpstreamData = 3,
  // HTTP response body for the response to an HTTP callout. Immutable.
  HttpCallResponseBody = 4,
  // Response data for the response to a gRPC callout. Immutable.
  GrpcReceiveBuffer = 5,
  // VM configuration data. Immutable.
  VmConfiguration = 6,
  // Plugin configuration data. Immutable.
  PluginConfiguration = 7,
  // Foreign function call argument data. Immutable.
  CallData = 8,
  MAX = 8,
};

// Flags values for `getBufferStatus` hostcall.
enum class WasmBufferFlags : int32_t {
  // These must be powers of 2.
  EndOfStream = 1,
};

// Stream type. Corresponds to Proxy-Wasm ABI `proxy_stream_type_t`.
enum class WasmStreamType : int32_t {
  // HTTP request.
  Request = 0,
  // HTTP response.
  Response = 1,
  // TCP(-like) data from downstream.
  Downstream = 2,
  // TCP(-like) data from upstream.
  Upstream = 3,
  MAX = 3,
};
