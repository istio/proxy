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

#ifndef AUTH_STORE_H
#define AUTH_STORE_H

#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"

#include "src/envoy/auth/config.h"
#include "src/envoy/auth/pubkey_cache.h"

namespace Envoy {
namespace Http {
namespace Auth {

// The JWT auth store object to store config and caches.
// For now it only has pubkey_cache. In the future it will have token cache.
// Tt is pre-thread and stored in thread local.
class JwtAuthStore : public ThreadLocal::ThreadLocalObject {
 public:
  // Load the config from envoy config.
  JwtAuthStore(const JwtAuthConfig& config)
      : config_(config), pubkey_cache_(config) {}

  // Get the JwtAuthConfig.
  const JwtAuthConfig& config() const { return config_; }

  // Get the pubkey cache.
  PubkeyCache& pubkey_cache() { return pubkey_cache_; }

 private:
  // Store the config.
  const JwtAuthConfig& config_;
  // The public key cache, indexed by issuer.
  PubkeyCache pubkey_cache_;
};

// The factory to create per-thread auth controller object.
class JwtAuthStoreFactory {
 public:
  JwtAuthStoreFactory(std::unique_ptr<JwtAuthConfig> config,
                      Server::Configuration::FactoryContext& context)
      : config_(std::move(config)), tls_(context.threadLocal().allocateSlot()) {
    const JwtAuthConfig& config_ref = *config_;
    tls_->set([&config_ref](Event::Dispatcher&)
                  -> ThreadLocal::ThreadLocalObjectSharedPtr {
                    return ThreadLocal::ThreadLocalObjectSharedPtr(
                        new JwtAuthStore(config_ref));
                  });
  }

  // Get per-thread auth controller object.
  JwtAuthStore& store() { return tls_->getTyped<JwtAuthStore>(); }

 private:
  // The auth config and own the object.
  std::unique_ptr<JwtAuthConfig> config_;
  // Thread local slot to store per-thread auth controller
  ThreadLocal::SlotPtr tls_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_STORE_H
