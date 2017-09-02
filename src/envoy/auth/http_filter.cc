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

#include "http_filter.h"
#include "config.h"
#include "jwt.h"

#include "common/http/message_impl.h"
#include "common/http/utility.h"
#include "envoy/http/async_client.h"
#include "server/config/network/http_connection_manager.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <string>

namespace Envoy {
namespace Http {

namespace {

std::string JsonToString(rapidjson::Document* d) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d->Accept(writer);
  return buffer.GetString();
}

}  // namespace

const LowerCaseString& JwtVerificationFilter::AuthorizedHeaderKey() {
  static LowerCaseString* key = new LowerCaseString("Istio-Auth-UserInfo");
  return *key;
}

JwtVerificationFilter::JwtVerificationFilter(
    std::shared_ptr<Auth::JwtAuthConfig> config)
    : config_(config) {}

JwtVerificationFilter::~JwtVerificationFilter() {}

void JwtVerificationFilter::onDestroy() {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
}

FilterHeadersStatus JwtVerificationFilter::decodeHeaders(HeaderMap& headers,
                                                         bool) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  state_ = Calling;
  stopped_ = false;

  /*
   * TODO: update cached public key regularly
   */

  // list up issuers whose public key should be fetched
  for (const auto& iss : config_->issuers_) {
    if (!iss->failed_ && !iss->loaded_) {
      calling_issuers_[iss->name_] = iss;
    }
  }
  // send HTTP requests to fetch public keys
  if (!calling_issuers_.empty()) {
    for (const auto& iss : config_->issuers_) {
      if (iss->failed_ || iss->loaded_) {
        continue;
      }
      iss->async_client_cb_ = std::unique_ptr<Auth::AsyncClientCallbacks>(
          new Auth::AsyncClientCallbacks(
              config_->cm_, iss->cluster_,
              [&](bool succeed, const std::string& pubkey) -> void {
                this->ReceivePubkey(headers, iss->name_, succeed, pubkey);
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
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {} Stop", __func__);
  stopped_ = true;
  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus JwtVerificationFilter::decodeData(Buffer::Instance&, bool) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  if (state_ == Calling) {
    return FilterDataStatus::StopIterationAndBuffer;
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus JwtVerificationFilter::decodeTrailers(HeaderMap&) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  if (state_ == Calling) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void JwtVerificationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  decoder_callbacks_ = &callbacks;
}

void JwtVerificationFilter::ReceivePubkey(HeaderMap& headers,
                                          std::string issuer_name, bool succeed,
                                          const std::string& pubkey) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {} , issuer = {}", __func__,
            issuer_name);
  auto iss_it = calling_issuers_.find(issuer_name);
  auto& iss = iss_it->second;
  iss->failed_ = !succeed;
  if (succeed) {
    iss->pkey_ = pubkey;
  }
  iss->loaded_ = true;
  calling_issuers_.erase(iss_it);

  // if receive all responses, proceed to verification
  if (calling_issuers_.empty()) {
    CompleteVerification(headers);
  }
}

/*
 * TODO: status as enum class
 */
std::string JwtVerificationFilter::Verify(HeaderMap& headers) {
  const HeaderEntry* entry = headers.get(kAuthorizationHeaderKey);
  if (!entry) {
    return "NO_AUTHORIZATION_HEADER";
  }
  const HeaderString& value = entry->value();
  if (strncmp(value.c_str(), kAuthorizationHeaderTokenPrefix.c_str(),
              kAuthorizationHeaderTokenPrefix.length()) != 0) {
    return "AUTHORIZATION_HEADER_BAD_FORMAT";
  }
  std::string jwt(value.c_str() + kAuthorizationHeaderTokenPrefix.length());

  for (const auto& iss : config_->issuers_) {
    if (iss->failed_) {
      continue;
    }

    /*
     * TODO: update according to change of JWT lib interface
     */
    // verifying and decoding JWT
    std::unique_ptr<rapidjson::Document> payload;
    if (iss->pkey_type_ == "pem") {
      payload = Auth::Jwt::Decode(jwt, iss->pkey_);
    } else if (iss->pkey_type_ == "jwks") {
      payload = Auth::Jwt::DecodeWithJwk(jwt, iss->pkey_);
    }

    if (payload) {
      // verification succeeded
      auto payload_str = JsonToString(payload.get());

      // Check the issuer's name.
      if (payload->HasMember("iss") && (*payload)["iss"].IsString() &&
          (*payload)["iss"].GetString() == iss->name_) {
        /*
         * TODO: check exp claim
         */

        /*
         * TODO: replace appropriately
         */
        headers.addReferenceKey(AuthorizedHeaderKey(), payload_str);

        // Remove JWT from headers.
        headers.remove(kAuthorizationHeaderKey);
        return "OK";
      }
    }
  }
  return "INVALID_SIGNATURE";
}

void JwtVerificationFilter::CompleteVerification(HeaderMap& headers) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  std::string status = Verify(headers);
  ENVOY_LOG(debug, "Verification status = {}", status);
  if (status != "OK") {
    // verification failed
    /*
     * TODO: detailed information on message body
     */
    Code code = Code(401);  // Unauthorized
    std::string message_body = "Verification Failed";
    Utility::sendLocalReply(*decoder_callbacks_, false, code, message_body);
    return;
  }

  state_ = Complete;
  if (stopped_) {
    decoder_callbacks_->continueDecoding();
  }
}

}  // Http
}  // Envoy
