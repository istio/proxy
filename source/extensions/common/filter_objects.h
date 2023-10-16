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

#include "envoy/registry/registry.h"
#include "envoy/stream_info/filter_state.h"
#include "source/common/router/string_accessor_impl.h"

namespace Envoy::Extensions::Common {

constexpr absl::string_view PeerPrincipalKey = "io.istio.peer_principal";
constexpr absl::string_view LocalPrincipalKey = "io.istio.local_principal";

class PeerPrincipalFactory : public StreamInfo::FilterState::ObjectFactory {
public:
  std::string name() const override { return std::string(PeerPrincipalKey); }
  std::unique_ptr<StreamInfo::FilterState::Object>
  createFromBytes(absl::string_view data) const override {
    return std::make_unique<Router::StringAccessorImpl>(data);
  }
};

class LocalPrincipalFactory : public StreamInfo::FilterState::ObjectFactory {
public:
  std::string name() const override { return std::string(LocalPrincipalKey); }
  std::unique_ptr<StreamInfo::FilterState::Object>
  createFromBytes(absl::string_view data) const override {
    return std::make_unique<Router::StringAccessorImpl>(data);
  }
};

REGISTER_FACTORY(LocalPrincipalFactory, StreamInfo::FilterState::ObjectFactory);
REGISTER_FACTORY(PeerPrincipalFactory, StreamInfo::FilterState::ObjectFactory);

} // namespace Envoy::Extensions::Common
