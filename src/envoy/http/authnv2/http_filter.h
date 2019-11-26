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
#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"
#include "envoy/http/filter.h"

// TODO(incfly): impl notes.
//
// input:
// envoy.jwt_authn {
//     "isser.google.com": {}
//     "isser.facebook.com": { iss:, aud: }
// }
// output:
// - see attribute_names, source.principle, source.namespace
// - all request.auth.* attributes
//
// can't add more attributes because of alignment...
// - Get rid of the config proto
//   - skip_trust_domain_verification...? get rid of it, worrying about it
//   later.
// - Get rid of the JWT authentication rejecting
// - Working with multi JWT.

// QA:
// Really the proto definition is just for composing data structure? Does not
// seem having the need to do cross-lang data encoding as normal proto does?
//
// The authenticator_base.h seems very much over-object oriented. no need for
// class inheritance.

// Where are the attributes generated? Before it's BuildCheckAttributes,
// request_handler? seems nothing to do with authn filter? where rbac filter is
// based on?
// Ans: it does. Write to authn filter entry. See the old one.

// What's lifecycle of the decodeHeaders/Trailers interaction?
// one req always `decodeHeaders` and then `decodebody`?
// multiple req share the same filter instance? bound by stream seems like?
// setting state, what does it mean? seems wrong by memorizing the state into
// filter if multi req handling?
namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

// The authentication filter.
class AuthenticationFilter : public StreamDecoderFilter,
                             public Logger::Loggable<Logger::Id::filter> {
 public:
  AuthenticationFilter() {}
  ~AuthenticationFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

  //  protected:
  // Convenient function to call decoder_callbacks_ only when stopped_ is true.
  // void continueDecoding();

  // Convenient function to reject request.
  // void rejectRequest(const std::string& message);

  // Creates peer authenticator. This is made virtual function for
  // testing.
  // virtual std::unique_ptr<Istio::AuthN::AuthenticatorBase>

  // createPeerAuthenticator(Istio::AuthN::FilterContext* filter_context);

  // // Creates origin authenticator.
  // virtual std::unique_ptr<Istio::AuthN::AuthenticatorBase>

  // createOriginAuthenticator(Istio::AuthN::FilterContext* filter_context);

 private:
  StreamDecoderFilterCallbacks* decoder_callbacks_{};

  enum State { INIT, PROCESSING, COMPLETE, REJECTED };
  State state_{State::INIT};

  // Context for authentication process. Created in decodeHeader to start
  // authentication process.
  // std::unique_ptr<Istio::AuthN::FilterContext> filter_context_;
};

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
