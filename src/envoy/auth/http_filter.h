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

#pragma once

#include "config.h"

#include "common/common/logger.h"
#include "server/config/network/http_connection_manager.h"

#include <map>
#include <memory>
#include <string>

namespace Envoy {
namespace Http {

class JwtVerificationFilter : public StreamDecoderFilter,
                              public Logger::Loggable<Logger::Id::http> {
 public:
  JwtVerificationFilter(std::shared_ptr<Auth::JwtAuthConfig> config);
  ~JwtVerificationFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

  const LowerCaseString kAuthorizationHeaderKey =
      LowerCaseString("Authorization");
  const std::string kAuthorizationHeaderTokenPrefix = "Bearer ";
  const LowerCaseString& AuthorizedHeaderKey();

 private:
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  std::shared_ptr<Auth::JwtAuthConfig> config_;

  enum State { Calling, Responded, Complete };
  State state_;
  bool stopped_;
  std::function<void(void)> cancel_verification_;

  // Struct to hold an issuer whose public key is being fetched, together with
  // the client making the request for its public key.
  struct CallingIssuerInfo {
    std::shared_ptr<Auth::IssuerInfo> iss_;
    std::unique_ptr<Auth::AsyncClientCallbacks> async_cb_;
  };

  // Map to keep the set of issuers whose public key is being fetched.
  // Key: Issuer's name
  std::map<std::string, CallingIssuerInfo> calling_issuers_;
  // Flag to check if expirations of all public keys are checked.
  bool all_issuers_pubkey_expiration_checked_;

  void ReceivePubkey(HeaderMap& headers, std::string issuer_name, bool succeed,
                     const std::string& pubkey);
  void LoadPubkeys(HeaderMap& headers);
  std::string Verify(HeaderMap& headers);
  void CompleteVerification(HeaderMap& headers);
};

}  // Http
}  // Envoy