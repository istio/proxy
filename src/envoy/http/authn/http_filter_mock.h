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

#include "gmock/gmock.h"
#include "src/envoy/http/authn/context.pb.h"
#include "src/envoy/http/authn/http_filter.h"

namespace Envoy {
namespace Http {

// POC for authN filter. In this implementation, the filter config is the
// original authN policy. Internally, it will trigger appropriate filters and/or
// validation routines to enforce the policy.
class MockAuthenticationFilter : public AuthenticationFilter {
 public:
  MockAuthenticationFilter(
      const istio::authentication::v1alpha1::Policy& proto_config)
      : AuthenticationFilter(proto_config) {}
  ~MockAuthenticationFilter(){};

  MOCK_CONST_METHOD3(validateX509,
                     void(const HeaderMap& headers,
                          const iaapi::MutualTls& params,
                          const AuthenticateDoneCallback& done_callback));
  MOCK_CONST_METHOD3(validateJwt,
                     void(const HeaderMap& headers, const iaapi::Jwt& params,
                          const AuthenticateDoneCallback& done_callback));
  const IstioAuthN::Context& context() { return context_; }
};

}  // namespace Http
}  // namespace Envoy
