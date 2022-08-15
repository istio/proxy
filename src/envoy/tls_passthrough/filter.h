// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "envoy/common/hashable.h"
#include "envoy/network/filter.h"
#include "envoy/ssl/connection.h"
#include "envoy/stream_info/filter_state.h"

namespace Istio {
namespace TLSPassthrough {

constexpr absl::string_view SslInfoFilterStateKey = "istio.passthrough_tls";

class SslInfoObject : public Envoy::StreamInfo::FilterState::Object,
                      public Envoy::Hashable {
 public:
  SslInfoObject(Envoy::Ssl::ConnectionInfoConstSharedPtr ssl_info)
      : ssl_info_(std::move(ssl_info)) {
    ASSERT(ssl_info_ != nullptr);
  }
  const Envoy::Ssl::ConnectionInfoConstSharedPtr& ssl() const {
    return ssl_info_;
  }
  // Envoy::Hashable
  absl::optional<uint64_t> hash() const override;

 private:
  const Envoy::Ssl::ConnectionInfoConstSharedPtr ssl_info_;
};

class BaseFilter : public Envoy::Network::ReadFilter,
                   public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
 public:
  // Envoy::Network::ReadFilter
  Envoy::Network::FilterStatus onData(Envoy::Buffer::Instance&, bool) override {
    return Envoy::Network::FilterStatus::Continue;
  }

  void initializeReadFilterCallbacks(
      Envoy::Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
  }

 protected:
  Envoy::Network::ReadFilterCallbacks* read_callbacks_{};
};

class CaptureTLSFilter : public BaseFilter {
  Envoy::Network::FilterStatus onNewConnection() override;
};

class RestoreTLSFilter : public BaseFilter {
  Envoy::Network::FilterStatus onNewConnection() override;
};

}  // namespace TLSPassthrough
}  // namespace Istio
