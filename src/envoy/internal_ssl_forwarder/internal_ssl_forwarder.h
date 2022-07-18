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

#include "envoy/network/filter.h"
#include "src/envoy/internal_ssl_forwarder/config/internal_ssl_forwarder.pb.h"

using namespace istio::telemetry::internal_ssl_forwarder;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace InternalSslForwarder {

class Config {
 public:
  Config(const v1::Config&) {}
};

using ConfigSharedPtr = std::shared_ptr<Config>;

class Filter : public Network::ReadFilter,
               Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(const ConfigSharedPtr&) {}

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  };

  // Network::ReadFilter
  Network::FilterStatus onNewConnection() override;

  // Network::ReadFilter
  void initializeReadFilterCallbacks(
      Network::ReadFilterCallbacks& callbacks) override {
    callbacks_ = &callbacks;
  };

 private:
  Network::ReadFilterCallbacks* callbacks_{};
};

}  // namespace InternalSslForwarder
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
