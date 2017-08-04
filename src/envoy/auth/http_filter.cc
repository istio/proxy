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

#include <string>

#include "config.h"
#include "http_filter.h"
#include "jwt.h"

#include "server/config/network/http_connection_manager.h"

namespace Envoy {
namespace Http {

const LowerCaseString& JwtVerificationFilter::headerKey() {
  static LowerCaseString* key = new LowerCaseString("Istio-Auth-UserInfo");
  return *key;
}

/*
 * temporary
 * TODO: replace appropriately
 */
const std::string& JwtVerificationFilter::headerValue() {
  static std::string* val = new std::string("success");
  return *val;
}

JwtVerificationFilter::JwtVerificationFilter(
    std::shared_ptr<Auth::JwtAuthConfig> config) {
  config_ = config;
}

JwtVerificationFilter::~JwtVerificationFilter() {}

void JwtVerificationFilter::onDestroy() {}

FilterHeadersStatus JwtVerificationFilter::decodeHeaders(HeaderMap& headers,
                                                         bool) {
  const HeaderEntry* entry = headers.get(LowerCaseString("Authorization"));
  if (entry) {
    const HeaderString& value = entry->value();
    const std::string bearer = "Bearer ";
    if (strncmp(value.c_str(), bearer.c_str(), bearer.length()) == 0) {
      std::string jwt(value.c_str() + bearer.length());

      for (const auto& iss : config_->issuers_) {
        const std::string& issuer_name = iss->name_;
        const std::string& type = iss->pkey_type_;
        const std::string& pkey = iss->pkey_;

        std::unique_ptr<rapidjson::Document> payload;
        if (type == "pem") {
          payload = Auth::Jwt::Decode(jwt, pkey);
        } else if (type == "jwks") {
          /*
           * TODO: implement
           */
        }

        if (payload) {
          if (payload->HasMember("iss") && (*payload)["iss"].IsString() &&
              (*payload)["iss"].GetString() == issuer_name) {
            /*
             * TODO: check exp claim
             */
            // verification success
            /*
             * TODO: add payload to HTTP header
             */
            /*
             * temporary
             * TODO: replace appropriately
             */
            headers.addStatic(headerKey(), headerValue());

            return FilterHeadersStatus::Continue;
          }
        }
      }
    }
  }
  /*
   * This is OK?
   * TODO: check about FilterHeaderStatus
   */
  return FilterHeadersStatus::Continue;
}

FilterDataStatus JwtVerificationFilter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus JwtVerificationFilter::decodeTrailers(HeaderMap&) {
  return FilterTrailersStatus::Continue;
}

void JwtVerificationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

}  // Http
}  // Envoy