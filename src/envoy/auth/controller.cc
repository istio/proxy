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

#include "controller.h"

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

// The per-request JWT authentication object.
class AuthRequest : public Logger::Loggable<Logger::Id::http>,
                    public std::enable_shared_from_this<AuthRequest> {
 public:
  AuthRequest(HttpGetFunc http_get_func, PubkeyCache& pubkey_cache,
              HeaderMap& headers, Controller::DoneFunc on_done)
      : http_get_func_(http_get_func),
        pubkey_cache_(pubkey_cache),
        headers_(headers),
        on_done_(on_done) {}

  // Verify a JWT token.
  CancelFunc Verify() {
    const HeaderEntry* entry = headers_.get(kAuthorizationKey);
    if (!entry) {
      // TODO: excludes some health checking paths
      return DoneWithStatus(Status::JWT_MISSED);
    }

    // Extract token from header.
    const HeaderString& value = entry->value();
    if (value.size() <= kBearerPrefix.length() ||
        strncmp(value.c_str(), kBearerPrefix.c_str(), kBearerPrefix.length()) !=
            0) {
      return DoneWithStatus(Status::BEARER_PREFIX_MISMATCH);
    }

    // Parse JWT token
    jwt_.reset(new Jwt(value.c_str() + kBearerPrefix.length()));
    if (jwt_->GetStatus() != Status::OK) {
      return DoneWithStatus(jwt_->GetStatus());
    }

    // Check "exp" claim.
    auto unix_timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (jwt_->Exp() < unix_timestamp) {
      return DoneWithStatus(Status::JWT_EXPIRED);
    }

    // Check the issuer is configured or not.
    auto issuer = pubkey_cache_.LookupByIssuer(jwt_->Iss());
    if (!issuer) {
      return DoneWithStatus(Status::JWT_UNKNOWN_ISSUER);
    }

    // Check if audience is allowed
    if (!issuer->config().IsAudienceAllowed(jwt_->Aud())) {
      return DoneWithStatus(Status::AUDIENCE_NOT_ALLOWED);
    }

    if (issuer->pubkey() && !issuer->Expired()) {
      return Verify(*issuer->pubkey());
    }

    auto pThis = GetPtr();
    return http_get_func_(issuer->config().uri, issuer->config().cluster,
                          [pThis](bool ok, const std::string& body) {
                            pThis->OnFetchPubkeyDone(ok, body);
                          });
  }

 private:
  // Get the shared_ptr from this object.
  std::shared_ptr<AuthRequest> GetPtr() { return shared_from_this(); }

  // Handle the verification done.
  CancelFunc DoneWithStatus(Status status) {
    on_done_(status);
    return nullptr;
  }

  // Verify with a specific public key.
  CancelFunc Verify(const Auth::Pubkeys& pubkey) {
    Auth::Verifier v;
    if (!v.Verify(*jwt_, pubkey)) {
      return DoneWithStatus(v.GetStatus());
    }

    headers_.addReferenceKey(kJwtPayloadKey, jwt_->PayloadStrBase64Url());

    // Remove JWT from headers.
    headers_.remove(kAuthorizationKey);
    return DoneWithStatus(Status::OK);
  }

  // Handle the public key fetch done event.
  CancelFunc OnFetchPubkeyDone(bool ok, const std::string& pubkey) {
    if (!ok) {
      return DoneWithStatus(Status::FAILED_FETCH_PUBKEY);
    }

    auto issuer = pubkey_cache_.LookupByIssuer(jwt_->Iss());
    Status status = issuer->SetKey(pubkey);
    if (status != Status::OK) {
      return DoneWithStatus(status);
    }

    return Verify(*issuer->pubkey());
  }

  // The transport function
  HttpGetFunc http_get_func_;
  // The pubkey cache object.
  PubkeyCache& pubkey_cache_;
  // The HTTP request headers
  HeaderMap& headers_;
  // The on_done function.
  Controller::DoneFunc on_done_;
  // The JWT object.
  std::unique_ptr<Auth::Jwt> jwt_;
};

}  // namespace

Controller::Controller(const Config& config, HttpGetFunc http_get_func)
    : http_get_func_(http_get_func), pubkey_cache_(config) {}

CancelFunc Controller::Verify(HeaderMap& headers, DoneFunc on_done) {
  auto request = std::make_shared<AuthRequest>(http_get_func_, pubkey_cache_,
                                               headers, on_done);
  return request->Verify();
}

const LowerCaseString& Controller::JwtPayloadKey() { return kJwtPayloadKey; }

ControllerFactory::ControllerFactory(
    std::unique_ptr<Config> config,
    Server::Configuration::FactoryContext& context)
    : config_(std::move(config)), tls_(context.threadLocal().allocateSlot()) {
  const Config& auth_config = *config_;
  auto http_get_func = NewHttpGetFuncByAsyncClient(context.clusterManager());
  tls_->set([&auth_config, http_get_func](
                Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
    return ThreadLocal::ThreadLocalObjectSharedPtr(
        new Controller(auth_config, http_get_func));
  });
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
