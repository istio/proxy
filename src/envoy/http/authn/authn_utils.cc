/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include "authn_utils.h"
#include "common/json/json_loader.h"
#include "src/envoy/http/jwt_auth/jwt.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {
// Extract JWT audience into the JwtPayload.
// Return true if "aud" is extracted. Otherwise, return false.
bool ExtractJwtAudience(const Envoy::Json::Object& obj,
                        istio::authn::JwtPayload* payload) {
  const std::string key = "aud";
  // "aud" can be either string array or string.
  // First, try as string, will throw execption if object type is not string.
  try {
    std::string obj_str = obj.getString(key);
    // Save "aud" to the payload
    payload->add_audiences(obj_str);
    return true;
  } catch (Json::Exception& e) {
    // Not convertable to string
  }
  // Next, try as string array
  try {
    std::vector<std::string> aud_vector = obj.getStringArray(key);
    for (size_t i = 0; i < aud_vector.size(); i++) {
      payload->add_audiences(aud_vector[i]);
    }
  } catch (Json::Exception& e) {
    // Not convertable to string array
    return false;
  }
  // return true to proceed to the next iteration.
  return true;
}
};  // namespace

bool AuthnUtils::GetJWTPayloadFromHeaders(
    const HeaderMap& headers, const LowerCaseString& jwt_payload_key,
    istio::authn::JwtPayload* payload) {
  const HeaderEntry* entry = headers.get(jwt_payload_key);
  if (!entry) {
    ENVOY_LOG(debug, "No JwtPayloadKey entry {} in the header",
              jwt_payload_key.get());
    return false;
  }
  std::string value(entry->value().c_str(), entry->value().size());
  // JwtAuth::Base64UrlDecode() is different from Base64::decode().
  std::string payload_str = JwtAuth::Base64UrlDecode(value);
  // Return an empty string if Base64 decode fails.
  if (payload_str.empty()) {
    ENVOY_LOG(error, "Invalid {} header, invalid base64: {}",
              jwt_payload_key.get(), value);
    return false;
  }
  ::google::protobuf::Map< ::std::string, ::std::string>* claims =
      payload->mutable_claims();
  try {
    auto json_obj = Json::Factory::loadFromString(payload_str);
    ENVOY_LOG(debug, "{}: json object is {}", __FUNCTION__,
              json_obj->asJsonString());
    // Extract claims
    json_obj->iterate(
        [payload](const std::string& key, const Json::Object& obj) -> bool {
          ::google::protobuf::Map< ::std::string, ::std::string>* claims =
              payload->mutable_claims();
          // In current implementation, only string objects are extracted into
          // claims. If call obj.asJsonString(), will get "panic: not reached"
          // from json_loader.cc.
          try {
            // Try as string, will throw execption if object type is not string.
            (*claims)[key] = obj.asString();
          } catch (Json::Exception& e) {
          }
          return true;
        });
    // Extract audience
    ExtractJwtAudience(*json_obj, payload);
  } catch (...) {
    return false;
  }

  if (payload->claims().empty()) {
    ENVOY_LOG(error, "{}: there is no JWT claims.", __func__);
    return false;
  }
  // Build user
  if (claims->count("iss") > 0 && claims->count("sub") > 0) {
    payload->set_user((*claims)["iss"] + "/" + (*claims)["sub"]);
  }
  // Build authorized presenter (azp)
  if (claims->count("azp") > 0) {
    payload->set_presenter((*claims)["azp"]);
  }
  return true;
}

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
