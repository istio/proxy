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
#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"
#include "src/envoy/http/authn/policy.pb.validate.h"

namespace Envoy {
namespace Http {
namespace Auth {

// The authentication store stores config and caches.
// It is per-thread and stored in thread local.
class AuthnStore : public ThreadLocal::ThreadLocalObject {
 public:
  AuthnStore(const istio::authentication::v1alpha1::Policy& config)
      : config_(config) {}

  // Get the Config.
  const istio::authentication::v1alpha1::Policy& config() const {
    return config_;
  }

 private:
  // Store the config.
  const istio::authentication::v1alpha1::Policy& config_;
};

// The factory to create a per-thread authentication store.
class AuthnStoreFactory : public Logger::Loggable<Logger::Id::config> {
 public:
  AuthnStoreFactory(const istio::authentication::v1alpha1::Policy& config,
                    Server::Configuration::FactoryContext& context)
      : config_(config), tls_(context.threadLocal().allocateSlot()) {
    tls_->set(
        [this](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
          return std::make_shared<AuthnStore>(config_);
        });
    ENVOY_LOG(info, "In AuthnStoreFactory, authentication policy config: {}",
              config_.DebugString());
  }

  // Get per-thread auth store object.
  AuthnStore& store() { return tls_->getTyped<AuthnStore>(); }

 private:
  // The authentication policy config.
  istio::authentication::v1alpha1::Policy config_;
  // Thread local slot to store per-thread auth store
  ThreadLocal::SlotPtr tls_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
