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

#include "envoy/network/filter.h"
#include "envoy/stats/scope.h"
#include "openssl/ssl.h"
#include "source/common/common/logger.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

/**
 * All stats for the SNI verifier. @see stats_macros.h
 */
#define SNI_VERIFIER_STATS(COUNTER)                                                                \
  COUNTER(client_hello_too_large)                                                                  \
  COUNTER(tls_found)                                                                               \
  COUNTER(tls_not_found)                                                                           \
  COUNTER(inner_sni_found)                                                                         \
  COUNTER(inner_sni_not_found)                                                                     \
  COUNTER(snis_do_not_match)

/**
 * Definition of all stats for the SNI verifier. @see stats_macros.h
 */
struct SniVerifierStats {
  SNI_VERIFIER_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * Global configuration for SNI verifier.
 */
class Config {
public:
  Config(Stats::Scope& scope, size_t max_client_hello_size = TLS_MAX_CLIENT_HELLO);

  const SniVerifierStats& stats() const { return stats_; }
  bssl::UniquePtr<SSL> newSsl();
  size_t maxClientHelloSize() const { return max_client_hello_size_; }

  static constexpr size_t TLS_MAX_CLIENT_HELLO = 64 * 1024;

private:
  SniVerifierStats stats_;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
  const size_t max_client_hello_size_;
};

typedef std::shared_ptr<Config> ConfigSharedPtr;

class Filter : public Network::ReadFilter, Logger::Loggable<Logger::Id::filter> {
public:
  Filter(const ConfigSharedPtr config);

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override { return Network::FilterStatus::Continue; }
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
  }

private:
  void parseClientHello(const void* data, size_t len);
  void done(bool success);
  void onServername(absl::string_view name);

  ConfigSharedPtr config_;
  Network::ReadFilterCallbacks* read_callbacks_{};

  bssl::UniquePtr<SSL> ssl_;
  uint64_t read_{0};
  bool clienthello_success_{false};
  bool done_{false};
  bool is_match_{false};
  bool restart_handshake_{false};

  std::unique_ptr<uint8_t[]> buf_;

  // Allows callbacks on the SSL_CTX to set fields in this class.
  friend class Config;
};

} // namespace SniVerifier
} // namespace Tcp
} // namespace Envoy
