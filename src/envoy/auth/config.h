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

#ifndef AUTH_CONFIG_H
#define AUTH_CONFIG_H

#include "envoy/json/json_object.h"
#include "src/envoy/auth/config.pb.h"

namespace Envoy {
namespace Http {
namespace Auth {

// A config for jwt-auth filter
class JwtAuthConfig {
 public:
  // Load the config from envoy config JSON object.
  JwtAuthConfig(const Json::Object& config);

  // Load config from proto serialized string.
  // Used to load config defined in:
  // https://github.com/istio/api/blob/master/mixer/v1/config/client/auth.proto
  JwtAuthConfig(const std::string& pb_str);

  // Get the config.
  const Config::AuthFilterConfig& config() const { return config_pb_; }

 private:
  Config::AuthFilterConfig config_pb_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_CONFIG_H
