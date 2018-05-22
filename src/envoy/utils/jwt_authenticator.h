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

#pragma once

#include "common/common/logger.h"
#include "envoy/http/async_client.h"

#include "src/envoy/utils/auth_store.h"
#include "src/envoy/utils/token_extractor.h"

namespace Envoy {
namespace Utils {
namespace Jwt {

class JwtAuthenticator : public Http::AsyncClient::Callbacks {
  public:
    // The callback interface to notify the completion event.
    class Callbacks {
      public:
      virtual ~Callbacks() {}
      virtual void onError(Status status) PURE;
      virtual void onSuccess(const Jwt *jwt, const Http::LowerCaseString *header) PURE;
    };
    virtual void onDestroy() PURE;
    virtual void Verify(std::unique_ptr<JwtTokenExtractor::Token> &token, JwtAuthenticator::Callbacks* callback) PURE;
    virtual void Verify(Http::HeaderMap& headers, Callbacks* callback) PURE;
};

// A per-request JWT authenticator to handle all JWT authentication:
// * fetch remote public keys and cache them.
class JwtAuthenticatorImpl : public JwtAuthenticator, public Logger::Loggable<Logger::Id::filter> {
 public:
  JwtAuthenticatorImpl(Upstream::ClusterManager& cm, JwtAuthStore& store);

  void Verify(std::unique_ptr<JwtTokenExtractor::Token> &token, JwtAuthenticator::Callbacks* callback) override;
  void Verify(Http::HeaderMap& headers, Callbacks* callback) override;

  // Called when the object is about to be destroyed.
  void onDestroy() override;

 private:
  // Fetch a remote public key.
  void FetchPubkey(PubkeyCacheItem* issuer);
  // Following two functions are for AyncClient::Callbacks
  void onSuccess(Http::MessagePtr&& response) override;
  void onFailure(Http::AsyncClient::FailureReason) override;

  // Verify with a specific public key.
  void VerifyKey(const PubkeyCacheItem& issuer);

  // Handle the public key fetch done event.
  void OnFetchPubkeyDone(const std::string& pubkey);

  // Calls the failed callback with status.
  void FailedWithStatus(const Status& status);

  // Calls the success callback with the JWT and token extractor
  void Success();

  // Return true if it is OK to forward this request without JWT.
  bool OkToBypass();

  // The cluster manager object to make HTTP call.
  Upstream::ClusterManager& cm_;
  // The cache object.
  JwtAuthStore& store_;
  // The JWT object.
  std::unique_ptr<Jwt> jwt_;
  // The token data
  std::unique_ptr<JwtTokenExtractor::Token> token_;
  // The on_done function.
  Callbacks* callback_{};

  // The pending uri_, only used for logging.
  std::string uri_;
  // The pending remote request so it can be canceled.
  Http::AsyncClient::Request* request_{};
};

}  // namespace Jwt
}  // namespace Utils
}  // namespace Envoy
