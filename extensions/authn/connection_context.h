/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#include "absl/types/optional.h"
#include "envoy/network/connection.h"

namespace Extensions {
namespace AuthN {

using Envoy::Network::Connection;

class ConnectionContext {
 public:
  virtual ~ConnectionContext() = default;

  // Peer or local trust domain.
  // It will return only `spiffe` prefixed domain.
  virtual absl::optional<std::string> trustDomain(bool peer) PURE;

  // Peer or local principal domain.
  // It will return arbitary domain which will extracted from SAN.
  virtual absl::optional<std::string> principalDomain(bool peer) PURE;

  // Check whether established connection enabled mTLS or not.
  virtual bool isMutualTls() const PURE;

  // Connection port.
  virtual absl::optional<uint32_t> port() const PURE;
};

class ConnectionContextImpl : public ConnectionContext {
 public:
  ConnectionContextImpl(const Connection* connection);

  // ConnectionContext
  absl::optional<std::string> trustDomain(bool peer) override;

  absl::optional<std::string> principalDomain(bool peer) override;

  bool isMutualTls() const override;

  absl::optional<uint32_t> port() const override;

 private:
  // Get SAN from peer or local TLS certificate. It will return first `spiffe`
  // prefixed SAN. If there is no `spiffe` prefixed SAN, it will return first
  // SAN.
  absl::optional<std::string> certSan(bool peer);

  const Connection* connection_;
};

using ConnectionContextPtr = std::shared_ptr<ConnectionContext>;

}  // namespace AuthN
}  // namespace Extensions