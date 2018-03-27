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
class JsonIterCallback : public Logger::Loggable<Logger::Id::filter> {
 public:
  // The json iterator callback function for extracting Jwt claims
  static bool JsonIteratorCallbackForJwtClaims(
      const std::string&, const Envoy::Json::Object&,
      istio::authn::JwtPayload* payload);
  // The json iterator callback function for exracting Jwt audience
  static bool JsonIteratorCallbackForJwtAudience(
      const std::string&, const Envoy::Json::Object&,
      istio::authn::JwtPayload* payload);
};

bool JsonIterCallback::JsonIteratorCallbackForJwtClaims(
    const std::string& key, const Envoy::Json::Object& obj,
    istio::authn::JwtPayload* payload) {
  ENVOY_LOG(debug, "{}: key is {}", __FUNCTION__, key);
  ::google::protobuf::Map< ::std::string, ::std::string>* claims =
      payload->mutable_claims();
  // In current implementation, only string objects are extracted into
  // claims. If call obj.asJsonString(), will get "panic: not reached" from
  // json_loader.cc.
  try {
    // Try as string, will throw execption if object type is not string.
    std::string obj_str = obj.asString();
    (*claims)[key] = obj_str;
  } catch (Json::Exception& e) {
  }
  return true;
}

bool JsonIterCallback::JsonIteratorCallbackForJwtAudience(
    const std::string& key, const Envoy::Json::Object& obj,
    istio::authn::JwtPayload* payload) {
  ENVOY_LOG(debug, "{}: key is {}", __FUNCTION__, key);
  // Only extract audience
  if (key != "aud") {
    return true;
  }
  // "aud" can be either string array or string.
  // First, try as string, will throw execption if object type is not string.
  try {
    std::string obj_str = obj.asString();
    // "aud" can be either string array or string.
    // Save it to the payload
    if (key == "aud") {
      payload->add_audiences(obj_str);
      return true;
    }
  } catch (Json::Exception& e) {
    // Not convertable to string
  }
  // Next, try as string array, read it as empty array if doesn't exist.
  try {
    // TODO (lei-tang): confirm the following code is a correct way to extract
    // the string array.
    std::string json_str = "{\"" + key + "\":" + obj.asJsonString() + "}";
    Envoy::Json::ObjectSharedPtr json_ptr =
        Envoy::Json::Factory::loadFromString(json_str);
    std::vector<std::string> aud_vector = json_ptr->getStringArray(key, true);
    for (size_t i = 0; i < aud_vector.size(); i++) {
      payload->add_audiences(aud_vector[i]);
    }
  } catch (Json::Exception& e) {
    ENVOY_LOG(error, "aud field type is not string or string array");
    ENVOY_LOG(error, "{}", e.what());
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
          return JsonIterCallback::JsonIteratorCallbackForJwtClaims(key, obj,
                                                                    payload);
        });
    // Extract audience
    json_obj->iterate(
        [payload](const std::string& key, const Json::Object& obj) -> bool {
          return JsonIterCallback::JsonIteratorCallbackForJwtAudience(key, obj,
                                                                      payload);
        });
  } catch (...) {
    ENVOY_LOG(error, "Invalid {} header, invalid json: {}",
              jwt_payload_key.get(), payload_str);
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
