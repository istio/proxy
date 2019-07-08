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
#include "common/protobuf/utility.h"
#include "envoy/config/filter/http/jwt_auth/v2alpha1/config.pb.h"
#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"
#include "src/envoy/http/jwt_auth/pubkey_cache.h"
#include "src/envoy/http/jwt_auth/token_extractor.h"

namespace Envoy {
namespace Http {
namespace JwtAuth {

typedef std::shared_ptr<const ::istio::envoy::config::filter::http::jwt_auth::
                            v2alpha1::JwtAuthentication>
    JwtAuthenticationConstSharedPtr;

// The JWT auth store object to store config and caches.
// It only has pubkey_cache for now. In the future it will have token cache.
// It is per-thread and stored in thread local.
class JwtAuthStore : public ThreadLocal::ThreadLocalObject {
 public:
  // Load the config from envoy config.
  JwtAuthStore(JwtAuthenticationConstSharedPtr config)
      : config_(config), pubkey_cache_(*config_), token_extractor_(*config_) {}

  // Get the Config.
  const ::istio::envoy::config::filter::http::jwt_auth::v2alpha1::
      JwtAuthentication&
      config() const {
    return *config_;
  }

  // Get the pubkey cache.
  PubkeyCache& pubkey_cache() { return pubkey_cache_; }

  // Get the private token extractor.
  const JwtTokenExtractor& token_extractor() const { return token_extractor_; }

 private:
  // Store the config.
  JwtAuthenticationConstSharedPtr config_;
  // The public key cache, indexed by issuer.
  PubkeyCache pubkey_cache_;
  // The object to extract token.
  JwtTokenExtractor token_extractor_;
};

// The factory to create per-thread auth store object.
class JwtAuthStoreFactory : public Logger::Loggable<Logger::Id::config> {
 public:
  JwtAuthStoreFactory(const ::istio::envoy::config::filter::http::jwt_auth::
                          v2alpha1::JwtAuthentication& config,
                      Server::Configuration::FactoryContext& context)
      : config_(std::make_shared<const ::istio::envoy::config::filter::http::
                                     jwt_auth::v2alpha1::JwtAuthentication>(
            config)),
        dummy_store_(config_),
        tls_(context.threadLocal().allocateSlot()) {
    tls_->set([config = this->config_](Event::Dispatcher&)
                  -> ThreadLocal::ThreadLocalObjectSharedPtr {
      return std::make_shared<JwtAuthStore>(config);
    });
    ENVOY_LOG(debug, "Loaded JwtAuthConfig: {}",
              MessageUtil::getJsonStringFromMessage(*config_, true));
  }

  // Get per-thread auth store object.
  JwtAuthStore& store() { return tls_->getTyped<JwtAuthStore>(); }

 private:
  // The auth config.
  JwtAuthenticationConstSharedPtr config_;
  // A dummy Auth store to verify config is valid
  JwtAuthStore dummy_store_;
  // Thread local slot to store per-thread auth store
  ThreadLocal::SlotPtr tls_;
};

}  // namespace JwtAuth
}  // namespace Http
}  // namespace Envoy
