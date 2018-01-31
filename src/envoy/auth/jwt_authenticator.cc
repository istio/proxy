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

#include "src/envoy/auth/jwt_authenticator.h"
#include "common/http/message_impl.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

// The autorization bearer prefix.
const std::string kBearerPrefix = "Bearer ";

// The HTTP header to pass verified token payload.
const LowerCaseString kJwtPayloadKey("sec-istio-auth-userinfo");

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

JwtAuthenticator::JwtAuthenticator(Upstream::ClusterManager& cm,
                                   JwtAuthStore& store)
    : cm_(cm), store_(store) {}

// Verify a JWT token.
void JwtAuthenticator::Verify(HeaderMap& headers,
                              JwtAuthenticator::Callbacks* callback) {
  headers_ = &headers;
  callback_ = callback;

  const HeaderEntry* entry = headers_->Authorization();
  if (!entry) {
    // TODO: excludes some health checking paths
    callback_->onDone(Status::JWT_MISSED);
    return;
  }

  // Extract token from header.
  const HeaderString& value = entry->value();
  if (!StringUtil::startsWith(value.c_str(), kBearerPrefix, true)) {
    callback_->onDone(Status::BEARER_PREFIX_MISMATCH);
    return;
  }

  // Parse JWT token
  jwt_.reset(new Jwt(value.c_str() + kBearerPrefix.length()));
  if (jwt_->GetStatus() != Status::OK) {
    callback_->onDone(jwt_->GetStatus());
    return;
  }

  // Check "exp" claim.
  const auto unix_timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  if (jwt_->Exp() < unix_timestamp) {
    callback_->onDone(Status::JWT_EXPIRED);
    return;
  }

  // Check the issuer is configured or not.
  auto issuer = store_.pubkey_cache().LookupByIssuer(jwt_->Iss());
  if (!issuer) {
    callback_->onDone(Status::JWT_UNKNOWN_ISSUER);
    return;
  }

  // Check if audience is allowed
  if (!issuer->IsAudienceAllowed(jwt_->Aud())) {
    callback_->onDone(Status::AUDIENCE_NOT_ALLOWED);
    return;
  }

  if (issuer->pubkey() && !issuer->Expired()) {
    VerifyKey(*issuer->pubkey());
    return;
  }

  FetchPubkey(issuer);
}

void JwtAuthenticator::FetchPubkey(PubkeyCacheItem* issuer) {
  uri_ = issuer->jwt_config().jwks_uri();
  std::string host, path;
  ExtractUriHostPath(uri_, &host, &path);

  MessagePtr message(new RequestMessageImpl());
  message->headers().insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Get);
  message->headers().insertPath().value(path);
  message->headers().insertHost().value(host);

  request_ = cm_.httpAsyncClientForCluster(
                    issuer->jwt_config().jwks_uri_envoy_cluster())
                 .send(std::move(message), *this,
                       Optional<std::chrono::milliseconds>());
}

void JwtAuthenticator::onSuccess(MessagePtr&& response) {
  request_ = nullptr;
  uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
  if (status_code == 200) {
    ENVOY_LOG(debug, "JwtAuthenticator [uri = {}]: success", uri_);
    std::string body;
    if (response->body()) {
      auto len = response->body()->length();
      body = std::string(static_cast<char*>(response->body()->linearize(len)),
                         len);
    } else {
      ENVOY_LOG(debug, "JwtAuthenticator [uri = {}]: body is empty", uri_);
    }
    OnFetchPubkeyDone(body);
  } else {
    ENVOY_LOG(debug, "JwtAuthenticator [uri = {}]: response status code {}",
              uri_, status_code);
    callback_->onDone(Status::FAILED_FETCH_PUBKEY);
  }
}

void JwtAuthenticator::onFailure(AsyncClient::FailureReason) {
  request_ = nullptr;
  ENVOY_LOG(debug, "JwtAuthenticator [uri = {}]: failed", uri_);
  callback_->onDone(Status::FAILED_FETCH_PUBKEY);
}

void JwtAuthenticator::onDestroy() {
  ENVOY_LOG(debug, "JwtAuthenticator [uri = {}]: canceled", uri_);
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }
}

// Handle the public key fetch done event.
void JwtAuthenticator::OnFetchPubkeyDone(const std::string& pubkey) {
  auto issuer = store_.pubkey_cache().LookupByIssuer(jwt_->Iss());
  Status status = issuer->SetKey(pubkey);
  if (status != Status::OK) {
    callback_->onDone(status);
  } else {
    VerifyKey(*issuer->pubkey());
  }
}

// Verify with a specific public key.
void JwtAuthenticator::VerifyKey(const Auth::Pubkeys& pubkey) {
  Auth::Verifier v;
  if (!v.Verify(*jwt_, pubkey)) {
    callback_->onDone(v.GetStatus());
    return;
  }

  headers_->addReferenceKey(kJwtPayloadKey, jwt_->PayloadStrBase64Url());

  // Remove JWT from headers.
  headers_->removeAuthorization();
  callback_->onDone(Status::OK);
}

const LowerCaseString& JwtAuthenticator::JwtPayloadKey() {
  return kJwtPayloadKey;
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
