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
namespace IstioAuthN {
enum State { INIT, PROCESSING, COMPLETE, REJECTED };
}  // namespace IstioAuthN

typedef std::function<void(std::unique_ptr<IstioAuthN::AuthenticatePayload>,
                           bool)>
    AuthenticateDoneCallback;

// The authentication filter.
class AuthenticationFilter : public StreamDecoderFilter,
                             public Logger::Loggable<Logger::Id::filter> {
 public:
  AuthenticationFilter(const istio::authentication::v1alpha1::Policy& config);
  ~AuthenticationFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

 protected:
  // Authenticate peer with the given method.
  void authenticatePeer(
      HeaderMap& headers,
      const istio::authentication::v1alpha1::PeerAuthenticationMethod& method,
      const AuthenticateDoneCallback& done_callback);

  // Callback for authenticatePeer.
  // If success is false (authn failed), this function will call the next
  // authentication method defined in the policy, if available; otherwise,
  // rejects the request (return 401).
  // If success is true (authn success), the source_user attribute will be set
  // from the result payload. It then populates the list of applicable methods
  // for origin authentication (which have at least one method) and then trigger
  // the authenticateOrigin flow.
  void onAuthenticatePeerDone(
      HeaderMap* headers, int source_method_index,
      std::unique_ptr<IstioAuthN::AuthenticatePayload> payload, bool success);

  // Authenticate origin using the given method.
  void authenticateOrigin(
      HeaderMap& headers,
      const istio::authentication::v1alpha1::OriginAuthenticationMethod& method,
      const AuthenticateDoneCallback& done_callback);

  // Call back for authenticateOrigin.
  // If success is false, this function will call the next authentication
  // method defined in the policy, if available; otherwise, rejects the
  // request (return 401).
  // If success is true, the origin payload will be set from the
  // result payload. Also, the principal is set from origin.user.
  void onAuthenticateOriginDone(
      HeaderMap* headers,
      const istio::authentication::v1alpha1::CredentialRule* rule,
      int method_index,
      std::unique_ptr<IstioAuthN::AuthenticatePayload> payload, bool success);

  // Validates x509 given the params (more or less, just check if x509 exists,
  // actual validation is not neccessary as it already done when the connection
  // establish), and extract authenticate attributes (just user/identity for
  // now). Calling callback with the extracted payload and corresponding status.
  virtual void validateX509(
      const HeaderMap& headers,
      const istio::authentication::v1alpha1::MutualTls& params,
      const AuthenticateDoneCallback& done_callback) const;

  // Validates JWT given the jwt params. If JWT is validated, it will call
  // the callback function with the extracted attributes and claims (JwtPayload)
  // and status SUCCESS. Otherwise, calling callback with status FAILED.
  virtual void validateJwt(const HeaderMap& headers,
                           const istio::authentication::v1alpha1::Jwt& params,
                           const AuthenticateDoneCallback& done_callback) const;

  // Convenient function to call decoder_callbacks_ only when stopped_ is true.
  void continueDecoding();

  // Convenient function to reject request.
  void rejectRequest(const std::string& message);

 protected:
  // Holds authentication attribute outputs.
  IstioAuthN::Context context_;

 private:
  // Store the config.
  const istio::authentication::v1alpha1::Policy& config_;

  StreamDecoderFilterCallbacks* decoder_callbacks_{};

  // Holds the state of the filter.
  IstioAuthN::State state_{IstioAuthN::State::INIT};

  // Indicates filter is 'stopped', thus (decoder_callbacks_) continueDecoding
  // should be called.
  bool stopped_{false};

  // Holds the list of methods that should be used for origin authentication.
  // This list is constructed at runtime, after source authentication success
  // (as it needs to know which credential rule to be applied, based on source
  // identity). The list, if constructed, should have at least one method.
  std::vector<
      const istio::authentication::v1alpha1::OriginAuthenticationMethod*>
      active_origin_methods_;
};

}  // namespace Http
}  // namespace Envoy
