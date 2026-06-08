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

#include "tools/worker/worker_protocol.h"

#include <nlohmann/json.hpp>

namespace bazel_rules_swift::worker_protocol {

// Populates an `Input` parsed from JSON. This function satisfies an API
// requirement of the JSON library, allowing it to automatically parse `Input`
// values from nested JSON objects.
void from_json(const ::nlohmann::json &j, Input &input) {
  // As with the protobuf messages from which these types originate, we supply
  // default values if any keys are not present.
  input.path = j.value("path", "");
  input.digest = j.value("digest", "");
}

// Populates an `WorkRequest` parsed from JSON. This function satisfies an API
// requirement of the JSON library (although `WorkRequest` is a top-level object
// in our schema so we only call it directly).
void from_json(const ::nlohmann::json &j, WorkRequest &work_request) {
  // As with the protobuf messages from which these types originate, we supply
  // default values if any keys are not present.
  work_request.arguments = j.value("arguments", std::vector<std::string>());
  work_request.inputs = j.value("inputs", std::vector<Input>());
  work_request.request_id = j.value("requestId", 0);
  work_request.cancel = j.value("cancel", false);
  work_request.verbosity = j.value("verbosity", 0);
  work_request.sandbox_dir = j.value("sandboxDir", "");
}

// Populates a JSON object with values from an `WorkResponse`. This function
// satisfies an API requirement of the JSON library (although `WorkResponse` is
// a top-level object in our schema so we only call it directly).
void to_json(::nlohmann::json &j, const WorkResponse &work_response) {
  j = ::nlohmann::json{{"exitCode", work_response.exit_code},
                       {"output", work_response.output},
                       {"requestId", work_response.request_id},
                       {"wasCancelled", work_response.was_cancelled}};
}

std::optional<WorkRequest> ReadWorkRequest(std::istream &stream) {
  std::string line;
  if (!std::getline(stream, line)) {
    return std::nullopt;
  }

  WorkRequest request;
  from_json(::nlohmann::json::parse(line), request);
  return request;
}

void WriteWorkResponse(const WorkResponse &response, std::ostream &stream) {
  ::nlohmann::json response_json;
  to_json(response_json, response);

  // Use `dump` with default arguments to get the most compact representation
  // of the response, and flush stdout after writing to ensure that Bazel
  // doesn't hang waiting for the response due to buffering.
  stream << response_json.dump() << std::flush;
}

}  // namespace bazel_rules_swift::worker_protocol
