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

#include "common/common/logger.h"

#include "extensions/filters/listener/tls_inspector/tls_inspector.h"

#include "openssl/bytestring.h"
#include "openssl/ssl.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

class NetworkLevelSniReaderFilter : public Network::ReadFilter,
                                    public Extensions::ListenerFilters::TlsInspector::TlsFilterBase,
                                    Logger::Loggable<Logger::Id::filter> {
public:
  NetworkLevelSniReaderFilter(
      const Extensions::ListenerFilters::TlsInspector::ConfigSharedPtr config);

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override { return Network::FilterStatus::Continue; }
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
  }

private:
  void done(bool success);
  // Extensions::ListenerFilters::TlsInspector::TlsFilterBase
  void onServername(absl::string_view name) override;
  void onALPN(const unsigned char*, unsigned int) override{};

  Extensions::ListenerFilters::TlsInspector::ConfigSharedPtr config_;
  Network::ReadFilterCallbacks* read_callbacks_{};

  bssl::UniquePtr<SSL> ssl_;
  uint64_t read_{0};
  bool alpn_found_{false};
  bool clienthello_success_{false};
  bool done_{false};

  static thread_local uint8_t
      buf_[Extensions::ListenerFilters::TlsInspector::Config::TLS_MAX_CLIENT_HELLO];
};

} // namespace NetworkLevelSniReader
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
