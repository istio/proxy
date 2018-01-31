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

#include "src/envoy/auth/config.h"
#include "common/common/logger.h"
#include "google/protobuf/util/json_util.h"

namespace Envoy {
namespace Http {
namespace Auth {

JwtAuthConfig::JwtAuthConfig(const Json::Object& config) {
  auto pb_str = config.asJsonString();
  google::protobuf::util::Status status =
      ::google::protobuf::util::JsonStringToMessage(pb_str, &config_pb_);
  if (!status.ok()) {
    throw EnvoyException(
        fmt::format("Failed to parse JSON config to proto: {}", pb_str));
  }
  auto& logger = Logger::Registry::getLog(Logger::Id::config);
  ENVOY_LOG_TO_LOGGER(logger, info, "Loaded JwtAuthConfig: {}",
                      config_pb_.DebugString());
}

JwtAuthConfig::JwtAuthConfig(const std::string& pb_str) {
  if (!config_pb_.ParseFromString(pb_str)) {
    throw EnvoyException(
        fmt::format("Failed to parse proto binary config: {}", pb_str));
  }
  auto& logger = Logger::Registry::getLog(Logger::Id::config);
  ENVOY_LOG_TO_LOGGER(logger, info, "Loaded JwtAuthConfig: {}",
                      config_pb_.DebugString());
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
