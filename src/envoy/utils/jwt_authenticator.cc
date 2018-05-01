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

#include "src/envoy/utils/jwt_authenticator.h"
#include "src/envoy/utils/jwt.h"
#include "common/http/message_impl.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Utils {
namespace Jwt {
namespace {

// Extract host and path from a URI
void ExtractUriHostPath(const std::string& uri, std::string* host,
                        std::string* path) {
  // Example:
  // uri  = "https://example.com/certs"
  // pos  :          ^
  // pos1 :                     ^
  // host = "example.com"
  // path = "/certs"
  auto pos = uri.find("://");
  pos = pos == std::string::npos ? 0 : pos + 3;  // Start position of host
  auto pos1 = uri.find("/", pos);
  if (pos1 == std::string::npos) {
    // If uri doesn't have "/", the whole string is treated as host.
    *host = uri.substr(pos);
    *path = "/";
  } else {
    *host = uri.substr(pos, pos1 - pos);
    *path = "/" + uri.substr(pos1 + 1);
  }
}

}  // namespace

JwtAuthenticatorImpl::JwtAuthenticatorImpl(Upstream::ClusterManager& cm,
                                   JwtAuthStore& store)
    : cm_(cm), store_(store) {}

// Verify JWT
void JwtAuthenticatorImpl::Verify(std::unique_ptr<JwtTokenExtractor::Token> &token, JwtAuthenticatorImpl::Callbacks* callback) {
    ENVOY_LOG(trace, "Jwt authentication from token starts");
    callback_ = callback;
    token_.swap(token);
    jwt_.reset(new Jwt(token_->token()));
    if (jwt_->GetStatus() != Status::OK) {
      FailedWithStatus(jwt_->GetStatus());
      return;
    }

    // Check if token is extracted from the location specified by the issuer.
    if (!token_->IsIssuerAllowed(jwt_->Iss())) {
      ENVOY_LOG(debug, "Token for issuer {} did not specify extract location",
                jwt_->Iss());
      FailedWithStatus(Status::JWT_UNKNOWN_ISSUER);
      return;
    }

    // Check the issuer is configured or not.
    auto issuer = store_.pubkey_cache().LookupByIssuer(jwt_->Iss());
    if (!issuer) {
      FailedWithStatus(Status::JWT_UNKNOWN_ISSUER);
      return;
    }

    // Check if audience is allowed
    if (!issuer->IsAudienceAllowed(jwt_->Aud())) {
      FailedWithStatus(Status::AUDIENCE_NOT_ALLOWED);
      return;
    }

    if (issuer->pubkey() && !issuer->Expired()) {
      VerifyKey(*issuer);
      return;
    }

    FetchPubkey(issuer);
}

// Verify a JWT token.
void JwtAuthenticatorImpl::Verify(Http::HeaderMap& headers,
                              JwtAuthenticatorImpl::Callbacks* callback) {
  ENVOY_LOG(trace, "Jwt authentication from headers starts");
  callback_ = callback;
  token_.reset(nullptr);

  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  store_.token_extractor().Extract(headers, &tokens);
  if (tokens.size() == 0) {
    FailedWithStatus(Status::JWT_MISSED);
    return;
  }

  // Select first token.
  Verify(tokens[0], callback);
}

void JwtAuthenticatorImpl::FetchPubkey(PubkeyCacheItem* issuer) {
  uri_ = issuer->jwt_config().remote_jwks().http_uri().uri();
  std::string host, path;
  ExtractUriHostPath(uri_, &host, &path);

  Http::MessagePtr message(new Http::RequestMessageImpl());
  message->headers().insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Get);
  message->headers().insertPath().value(path);
  message->headers().insertHost().value(host);

  const auto& cluster = issuer->jwt_config().remote_jwks().http_uri().cluster();
  if (cm_.get(cluster) == nullptr) {
    FailedWithStatus(Status::FAILED_FETCH_PUBKEY);
    return;
  }

  ENVOY_LOG(debug, "fetch pubkey from [uri = {}]: start", uri_);
  request_ = cm_.httpAsyncClientForCluster(cluster).send(
      std::move(message), *this, absl::optional<std::chrono::milliseconds>());
}

void JwtAuthenticatorImpl::onSuccess(Http::MessagePtr&& response) {
  request_ = nullptr;
  uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
  if (status_code == 200) {
    ENVOY_LOG(debug, "fetch pubkey [uri = {}]: success", uri_);
    std::string body;
    if (response->body()) {
      auto len = response->body()->length();
      body = std::string(static_cast<char*>(response->body()->linearize(len)),
                         len);
    } else {
      ENVOY_LOG(debug, "fetch pubkey [uri = {}]: body is empty", uri_);
    }
    OnFetchPubkeyDone(body);
  } else {
    ENVOY_LOG(debug, "fetch pubkey [uri = {}]: response status code {}", uri_,
              status_code);
    FailedWithStatus(Status::FAILED_FETCH_PUBKEY);
  }
}

void JwtAuthenticatorImpl::onFailure(Http::AsyncClient::FailureReason) {
  request_ = nullptr;
  ENVOY_LOG(debug, "fetch pubkey [uri = {}]: failed", uri_);
  FailedWithStatus(Status::FAILED_FETCH_PUBKEY);
}

void JwtAuthenticatorImpl::onDestroy() {
  if (request_) {
    request_->cancel();
    request_ = nullptr;
    ENVOY_LOG(debug, "fetch pubkey [uri = {}]: canceled", uri_);
  }
}

// Handle the public key fetch done event.
void JwtAuthenticatorImpl::OnFetchPubkeyDone(const std::string& pubkey) {
  auto issuer = store_.pubkey_cache().LookupByIssuer(jwt_->Iss());
  Status status = issuer->SetRemoteJwks(pubkey);
  if (status != Status::OK) {
    FailedWithStatus(status);
  } else {
    VerifyKey(*issuer);
  }
}

// Verify with a specific public key.
void JwtAuthenticatorImpl::VerifyKey(const PubkeyCacheItem& issuer_item) {
  Verifier v;
  if (!v.Verify(*jwt_, *issuer_item.pubkey())) {
    FailedWithStatus(v.GetStatus());
  } else {
    Success();
  }
}

void JwtAuthenticatorImpl::FailedWithStatus(const Status& status) {
  ENVOY_LOG(debug, "Jwt authentication failed with status: {}",
            StatusToString(status));
  callback_->onError(status);
  callback_ = nullptr;
}

void JwtAuthenticatorImpl::Success() {
  ENVOY_LOG(debug, "Jwt authentication Succeeded");
  callback_->onSuccess(jwt_.get(), token_->header());
  callback_ = nullptr;
}

}  // namespace Jwt
}  // namespace Utils
}  // namespace Envoy
