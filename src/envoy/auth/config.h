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

#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include "envoy/json/json_object.h"

#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

// A config for Jwt Auth filter
// struct JwtAuthConfig{
//  void Load(const Json::Object& json);
//};

class IssuerInfo {
 public:
  std::string name_;
  std::string pkey_type_;
  std::string pkey_;
  IssuerInfo(const std::string &name, const std::string &pkey_type,
             const std::string &pkey);
};

class JwtAuthConfig {
 public:
  JwtAuthConfig(const Json::Object &config) { Load(config); }
  std::vector<std::shared_ptr<IssuerInfo> > issuers_;

 private:
  void Load(const Json::Object &json);
  std::shared_ptr<IssuerInfo> LoadIssuer(Json::Object *json);
  std::shared_ptr<IssuerInfo> LoadIssuerFromDiscoveryDocment(
      Json::Object *json);
  std::shared_ptr<IssuerInfo> LoadPubkeyFromObject(Json::Object *json);
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // PROXY_CONFIG_H
