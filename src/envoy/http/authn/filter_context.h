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

#include "authentication/v1alpha1/policy.pb.h"
#include "common/common/logger.h"
#include "server/config/network/http_connection_manager.h"
#include "src/envoy/http/authn/context.pb.h"

namespace Envoy {
namespace Http {

class FilterContext : public Logger::Loggable<Logger::Id::filter> {
 public:
  FilterContext();
  virtual ~FilterContext();

  virtual const Network::Connection* connection() const PURE;

  void setPeerResult(const IstioAuthN::Payload* payload);
  void setOriginResult(const IstioAuthN::Payload* payload);
  void setPrincipal(
      istio::authentication::v1alpha1::CredentialRule::Binding binding);

  const IstioAuthN::Result& authenticationResult() { return result_; }

  void setHeaders(HeaderMap* headers);
  HeaderMap* headers();

 private:
  // Holds authentication attribute outputs.
  IstioAuthN::Result result_;

  HeaderMap* headers_;
};

}  // namespace Http
}  // namespace Envoy
