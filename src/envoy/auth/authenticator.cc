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

#include "src/envoy/auth/authenticator.h"
#include "common/http/message_impl.h"

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

// The authorization HTTP header.
const LowerCaseString kAuthorizationKey("authorization");

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
  if (pos1 == std::string::npos) pos1 = uri.length();
  *host = uri.substr(pos, pos1 - pos);
  *path = "/" + uri.substr(pos1 + 1);
}

}  // namespace

Authenticator::Authenticator(Upstream::ClusterManager& cm, JwtAuthStore& store)
    : cm_(cm), store_(store) {}

// Verify a JWT token.
void Authenticator::Verify(HeaderMap& headers, Authenticator::Callbacks* cb) {
  headers_ = &headers;
  cb_ = cb;

  const HeaderEntry* entry = headers_->get(kAuthorizationKey);
  if (!entry) {
    // TODO: excludes some health checking paths
    cb_->onDone(Status::JWT_MISSED);
    return;
  }

  // Extract token from header.
  const HeaderString& value = entry->value();
  if (value.size() <= kBearerPrefix.length() ||
      strncmp(value.c_str(), kBearerPrefix.c_str(), kBearerPrefix.length()) !=
          0) {
    cb_->onDone(Status::BEARER_PREFIX_MISMATCH);
    return;
  }

  // Parse JWT token
  jwt_.reset(new Jwt(value.c_str() + kBearerPrefix.length()));
  if (jwt_->GetStatus() != Status::OK) {
    cb_->onDone(jwt_->GetStatus());
    return;
  }

  // Check "exp" claim.
  auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  if (jwt_->Exp() < unix_timestamp) {
    cb_->onDone(Status::JWT_EXPIRED);
    return;
  }

  // Check the issuer is configured or not.
  auto issuer = store_.pubkey_cache().LookupByIssuer(jwt_->Iss());
  if (!issuer) {
    cb_->onDone(Status::JWT_UNKNOWN_ISSUER);
    return;
  }

  // Check if audience is allowed
  if (!issuer->issuer_config().IsAudienceAllowed(jwt_->Aud())) {
    cb_->onDone(Status::AUDIENCE_NOT_ALLOWED);
    return;
  }

  if (issuer->pubkey() && !issuer->Expired()) {
    VerifyKey(*issuer->pubkey());
    return;
  }

  FetchPubkey(issuer);
}

void Authenticator::FetchPubkey(PubkeyCacheItem* issuer) {
  uri_ = issuer->issuer_config().uri;
  std::string host, path;
  ExtractUriHostPath(uri_, &host, &path);

  MessagePtr message(new RequestMessageImpl());
  message->headers().insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Get);
  message->headers().insertPath().value(path);
  message->headers().insertHost().value(host);

  request_ = cm_.httpAsyncClientForCluster(issuer->issuer_config().cluster)
                 .send(std::move(message), *this,
                       Optional<std::chrono::milliseconds>());
}

void Authenticator::onSuccess(MessagePtr&& response) {
  request_ = nullptr;
  std::string status = response->headers().Status()->value().c_str();
  if (status == "200") {
    ENVOY_LOG(debug, "Authenticator [uri = {}]: success", uri_);
    std::string body;
    if (response->body()) {
      auto len = response->body()->length();
      body = std::string(static_cast<char*>(response->body()->linearize(len)),
                         len);
    } else {
      ENVOY_LOG(debug, "Authenticator [uri = {}]: body is empty", uri_);
    }
    OnFetchPubkeyDone(body);
  } else {
    ENVOY_LOG(debug, "Authenticator [uri = {}]: response status code {}", uri_,
              status);
    cb_->onDone(Status::FAILED_FETCH_PUBKEY);
  }
}

void Authenticator::onFailure(AsyncClient::FailureReason) {
  request_ = nullptr;
  ENVOY_LOG(debug, "Authenticator [uri = {}]: failed", uri_);
  cb_->onDone(Status::FAILED_FETCH_PUBKEY);
}

void Authenticator::onDestroy() {
  ENVOY_LOG(debug, "Authenticator [uri = {}]: canceled", uri_);
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }
}

// Handle the public key fetch done event.
void Authenticator::OnFetchPubkeyDone(const std::string& pubkey) {
  auto issuer = store_.pubkey_cache().LookupByIssuer(jwt_->Iss());
  Status status = issuer->SetKey(pubkey);
  if (status != Status::OK) {
    cb_->onDone(status);
  } else {
    VerifyKey(*issuer->pubkey());
  }
}

// Verify with a specific public key.
void Authenticator::VerifyKey(const Auth::Pubkeys& pubkey) {
  Auth::Verifier v;
  if (!v.Verify(*jwt_, pubkey)) {
    cb_->onDone(v.GetStatus());
    return;
  }

  headers_->addReferenceKey(kJwtPayloadKey, jwt_->PayloadStrBase64Url());

  // Remove JWT from headers.
  headers_->remove(kAuthorizationKey);
  cb_->onDone(Status::OK);
}

const LowerCaseString& Authenticator::JwtPayloadKey() { return kJwtPayloadKey; }

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
