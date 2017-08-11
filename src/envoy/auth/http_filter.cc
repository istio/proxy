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

#include "common/http/message_impl.h"
#include "envoy/http/async_client.h"
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
  printf("\n%s\n", __func__);

  state_ = Calling;

  // list up issuers whose public key should be fetched
  for (const auto& iss : config_->issuers_) {
    if (!iss->failed_ && !iss->loaded_) {
      calling_pubkeys_[iss->name()] = iss;
    }
  }
  // send HTTP requests to fetch public keys
  if (!calling_pubkeys_.empty()) {
    for (const auto& iss : config_->issuers_) {
      if (iss->failed_ || iss->loaded_) {
        continue;
      }
      iss->async_client_cb_ = std::unique_ptr<Auth::AsyncClientCallbacks>(
          new Auth::AsyncClientCallbacks(
              config_->cm_, iss->cluster_,
              [&](bool succeed, const std::string& pubkey) -> void {
                this->ReceivePubkey(headers, iss->name(), succeed, pubkey);
              }));

      iss->async_client_cb_->Call(iss->uri_);
    }
  } else {
    // If we do not need to fetch any public keys, just proceed to verification.
    CompleteVerification(headers);
  }

  if (state_ == Complete) {
    return FilterHeadersStatus::Continue;
  }
  stopped_ = true;
  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus JwtVerificationFilter::decodeData(Buffer::Instance&, bool) {
  printf("\n%s\n", __func__);
  if (state_ == Calling) {
    return FilterDataStatus::StopIterationAndBuffer;
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus JwtVerificationFilter::decodeTrailers(HeaderMap&) {
  printf("\n%s\n", __func__);
  if (state_ == Calling) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void JwtVerificationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  printf("\n%s\n", __func__);
  decoder_callbacks_ = &callbacks;
}

void JwtVerificationFilter::ReceivePubkey(HeaderMap& headers,
                                          std::string issuer_name, bool succeed,
                                          const std::string& pubkey) {
  printf("\n%s\n", __func__);

  auto iss_it = calling_pubkeys_.find(issuer_name);
  iss_it->second->failed_ = !succeed;
  if (succeed) {
    iss_it->second->pkey_ = pubkey;
  }
  iss_it->second->loaded_ = true;
  calling_pubkeys_.erase(iss_it);

  // if receive all responses, proceed to verification
  if (calling_pubkeys_.empty()) {
    CompleteVerification(headers);
    if (stopped_) {
      decoder_callbacks_->continueDecoding();
    }
  }
}

void JwtVerificationFilter::CompleteVerification(HeaderMap& headers) {
  printf("\n%s\n", __func__);

  const HeaderEntry* entry = headers.get(LowerCaseString("Authorization"));
  if (entry) {
    const HeaderString& value = entry->value();
    const std::string bearer = "Bearer ";
    if (strncmp(value.c_str(), bearer.c_str(), bearer.length()) == 0) {
      std::string jwt(value.c_str() + bearer.length());

      for (const auto& iss : config_->issuers_) {
        if (iss->failed_) {
          continue;
        }
        const std::string& issuer_name = iss->name();
        const std::string& type = iss->pkey_type();
        const std::string& pkey = iss->pkey();

        /*
         * TODO: check JWT's issuer
         */

        // verifying and decoding JWT
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
            //            headers.addStatic(headerKey(), headerValue());
            headers.addReferenceKey(headerKey(), headerValue());
          }
        }
      }
    }
  }
  state_ = Complete;
  /*
   * TODO: the case if verification failed
   */
}

}  // Http
}  // Envoy