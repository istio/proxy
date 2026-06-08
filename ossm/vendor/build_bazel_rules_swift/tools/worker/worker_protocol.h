// Copyright 2022 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_WORKER_PROTOCOL_H_
#define BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_WORKER_PROTOCOL_H_

#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace bazel_rules_swift::worker_protocol {

// An input file passed into a work request.
//
// This struct corresponds to the `blaze.worker.Input` message defined in
// https://github.com/bazelbuild/bazel/blob/master/src/main/protobuf/worker_protocol.proto.
struct Input {
  // The path in the file system from which the file should be read.
  std::string path;

  // An opaque token representing a hash of the file's contents.
  std::string digest;
};

// A single work unit that Bazel sent to the worker.
//
// This struct corresponds to the `blaze.worker.WorkRequest` message defined in
// https://github.com/bazelbuild/bazel/blob/master/src/main/protobuf/worker_protocol.proto.
struct WorkRequest {
  // The command line arguments of the action.
  std::vector<std::string> arguments;

  // The inputs that the worker is allowed to read during execution of this
  // request.
  std::vector<Input> inputs;

  // If 0, this request must be processed alone; otherwise, it is the unique
  // identifier of a request that can be processed in parallel with other
  // requests.
  int request_id;

  // If true, a previously sent `WorkRequest` with the same request ID should be
  // cancelled.
  bool cancel;

  // If greater than zero, the worker may output extra debug information to the
  // worker log via stderr.
  int verbosity;

  // For multiplex workers, this is the relative path inside the worker's
  // current working directory where the worker can place inputs and outputs.
  // This is empty for singleplex workers, which use their current working
  // directory directly.
  std::string sandbox_dir;
};

// A message sent from the worker back to Bazel when it has finished its work on
// a request.
//
// This struct corresponds to the `blaze.worker.WorkResponse` message defined in
// https://github.com/bazelbuild/bazel/blob/master/src/main/protobuf/worker_protocol.proto.
struct WorkResponse {
  // The exit status to report for the action.
  int exit_code;

  // Text printed to the user after the response has been received (for example,
  // compiler warnings/errors).
  std::string output;

  // The ID of the `WorkRequest` that this response is associated with.
  int request_id;

  // Indicates that the corresponding request was cancelled.
  bool was_cancelled;
};

// Parses and returns the next `WorkRequest` from the given stream. The format
// of the stream must be newline-delimited JSON (i.e., each line of the input is
// a complete JSON object). This function returns `nullopt` if the request could
// not be read (for example, because the JSON was malformed, or the stream was
// closed).
std::optional<WorkRequest> ReadWorkRequest(std::istream &stream);

// Writes the given `WorkResponse` as compact JSON to the given stream.
void WriteWorkResponse(const WorkResponse &response, std::ostream &stream);

}  // namespace bazel_rules_swift::worker_protocol

#endif  // BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_WORKER_PROTOCOL_H_
