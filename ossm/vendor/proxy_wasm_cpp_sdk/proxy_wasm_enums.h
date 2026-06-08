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
 * Intrinsic enumerations available to WASM modules.
 */
// NOLINT(namespace-envoy)
#pragma once

#include <cstdint>

// Severity levels for logging operations.
enum class LogLevel : int32_t { trace, debug, info, warn, error, critical, Max = critical };

// Enum indicating whether to continue processing of a TCP(-like) stream
// following a callback.
enum class FilterStatus : int32_t {
  // The host should continue to process the stream.
  Continue = 0,
  // The host should suspend further processing of the stream until plugin code
  // unpauses the stream via a call to `continueDownstream` or
  // `continueUpstream`.
  StopIteration = 1
};

// Enum indicating whether to continue processing of a stream following an HTTP
// header-related callback.
enum class FilterHeadersStatus : int32_t {
  // The host should continue to process the stream.
  Continue = 0,
  // The host should suspend further processing of headers until plugin code
  // unpauses the stream via a call to `continueRequest` or `continueResponse`.
  // Some host implementations may treat this equivalently to
  // `StopAllIterationAndWatermark`.
  StopIteration = 1,
  // The host should terminate the stream.
  ContinueAndEndStream = 2,
  // The host should suspend further processing of the stream until plugin code
  // unpauses the stream via a call to `continueRequest` or `continueResponse`,
  // in the meantime buffering all body bytes received, subject to host limits.
  StopAllIterationAndBuffer = 3,
  // The host should suspend further processing of the stream including reading
  // body data until plugin code unpauses the stream via a call to
  // `continueRequest` or `continueResponse`.
  StopAllIterationAndWatermark = 4,
};

// Enum indicating whether to continue processing of a stream following a
// metadata-related callback.
enum class FilterMetadataStatus : int32_t {
  // The host should continue to process the stream.
  Continue = 0
};

// Enum indicating whether to continue processing of a stream following an HTTP
// trailer-related callback.
enum class FilterTrailersStatus : int32_t {
  // The host should continue to process the stream.
  Continue = 0,
  // The host should suspend further processing of the stream until plugin code
  // unpauses the stream via a call to `continueRequest` or `continueResponse`.
  StopIteration = 1
};

// Enum indicating whether to continue processing of a stream following an HTTP
// body-related callback.
enum class FilterDataStatus : int32_t {
  // The host should continue to process the stream.
  Continue = 0,
  // The host should suspend further processing of the stream until plugin code
  // unpauses the stream via a call to `continueRequest` or `continueResponse`,
  // in the meantime buffering all body bytes received, subject to host limits.
  StopIterationAndBuffer = 1,
  // The host should suspend further processing of the stream including reading
  // body data until plugin code unpauses the stream via a call to
  // `continueRequest` or `continueResponse`.
  StopIterationAndWatermark = 2,
  // The host should suspend further processing of the stream other than
  // receiving body data until plugin code unpauses the stream via a call to
  // `continueRequest` or `continueResponse`.
  StopIterationNoBuffer = 3
};

// gRPC status codes.
enum class GrpcStatus : int32_t {
  Ok = 0,
  Canceled = 1,
  Unknown = 2,
  InvalidArgument = 3,
  DeadlineExceeded = 4,
  NotFound = 5,
  AlreadyExists = 6,
  PermissionDenied = 7,
  ResourceExhausted = 8,
  FailedPrecondition = 9,
  Aborted = 10,
  OutOfRange = 11,
  Unimplemented = 12,
  Internal = 13,
  Unavailable = 14,
  DataLoss = 15,
  Unauthenticated = 16,
  MaximumValid = Unauthenticated,
  InvalidCode = -1
};

// Types of metrics.
enum class MetricType : int32_t {
  // Value representing a cumulative count of some event.
  Counter = 0,
  // Value representing a current snapshot of some quantity.
  Gauge = 1,
  // Bucketed distribution of values.
  Histogram = 2,
  Max = 2,
};

// Enum indicating how a connection was closed.
enum class CloseType : int32_t {
  Unknown = 0,
  // Close initiated by the proxy.
  Local = 1,
  // Close initiated by the peer.
  Remote = 2,
};
