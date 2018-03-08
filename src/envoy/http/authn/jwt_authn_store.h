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

#include "authentication/v1alpha1/policy.pb.h"
#include "common/common/logger.h"
#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"
#include "src/envoy/http/authn/jwt_pubkey_cache.h"

namespace Envoy {
namespace Http {
namespace IstioAuthN {

// The JWT authn store object to store config and caches.
// It only has pubkey_cache for now. In the future it will have token cache.
// It is per-thread and stored in thread local.
class JwtAuthnStore : public ThreadLocal::ThreadLocalObject {
 public:
  // Load the config from envoy config.
  JwtAuthnStore(const istio::authentication::v1alpha1::Policy& config)
      : config_(config), pubkey_cache_() {}

  // Get the Config.
  const istio::authentication::v1alpha1::Policy& config() const {
    return config_;
  }

  // Get the pubkey cache.
  JwtPubkeyCache& pubkey_cache() { return pubkey_cache_; }

 private:
  // Store the config.
  const istio::authentication::v1alpha1::Policy& config_;
  // The JWT public key cache, indexed by issuer.
  JwtPubkeyCache pubkey_cache_;
};

// The factory to create per-thread JWT authn store object.
class JwtAuthnStoreFactory : public Logger::Loggable<Logger::Id::config> {
 public:
  JwtAuthnStoreFactory(const istio::authentication::v1alpha1::Policy& config,
                       Server::Configuration::FactoryContext& context)
      : config_(config), tls_(context.threadLocal().allocateSlot()) {
    tls_->set(
        [this](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
          return std::make_shared<JwtAuthnStore>(config_);
        });
    ENVOY_LOG(info, "Loaded JwtAuthnConfig: {}", config_.DebugString());
  }

  // Get per-thread auth store object.
  JwtAuthnStore& store() { return tls_->getTyped<JwtAuthnStore>(); }

 private:
  // The auth config.
  istio::authentication::v1alpha1::Policy config_;
  // Thread local slot to store per-thread auth store
  ThreadLocal::SlotPtr tls_;
};

}  // namespace IstioAuthN
}  // namespace Http
}  // namespace Envoy
