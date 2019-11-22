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

#include "src/envoy/http/authnv2/http_filter.h"

#include "authentication/v1alpha1/policy.pb.h"
#include "common/http/utility.h"
#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"
#include "include/istio/utils/attribute_names.h"
#include "src/envoy/http/authnv2/origin_authenticator.h"
#include "src/envoy/http/authnv2/peer_authenticator.h"
#include "src/envoy/utils/authn.h"
#include "src/envoy/utils/filter_names.h"
#include "src/envoy/utils/utils.h"

#include <string>

using istio::authn::Payload;
using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

AuthenticationFilter::~AuthenticationFilter() {}

void AuthenticationFilter::onDestroy() {
  ENVOY_LOG(debug, "Called AuthenticationFilter : {}", __func__);
}

// Helper function to set a key/value pair into Struct.
static void setKeyValueFoo(::google::protobuf::Struct& data, std::string key,
                           std::string value) {
  (*data.mutable_fields())[key].set_string_value(value);
}

FilterHeadersStatus AuthenticationFilter::decodeHeaders(HeaderMap&, bool) {
  ENVOY_LOG(debug, "AuthenticationFilter::decodeHeaders start\n");
  state_ = State::PROCESSING;

  // populate peer identity from x509 cert.
  const Network::Connection* connection = decoder_callbacks_->connection();
  // Always try to get principal and set to output if available.
  std::string peer_principle;
  if (connection->ssl() != nullptr &&
      connection->ssl()->peerCertificatePresented()) {
    Utils::GetPrincipal(connection, true, &peer_principle);
  }
  ENVOY_LOG(info, "incfly debug peer principle {}", peer_principle);
  auto metadata = decoder_callbacks_->streamInfo().dynamicMetadata();
  ProtobufWkt::Struct auth_attr;
  setKeyValueFoo(auth_attr, istio::utils::AttributeName::kSourcePrincipal,
                 peer_principle);
  decoder_callbacks_->streamInfo().setDynamicMetadata(
      Utils::IstioFilterName::kAuthentication, auth_attr);

  // Get request.authn attributes from Jwt filter metadata.
  auto filter_it = metadata.filter_metadata().find(
      Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn);
  if (filter_it != metadata.filter_metadata().end()) {
    ENVOY_LOG(info, "No dynamic_metadata found for filter {}",
              Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn);
    // Iterate over the envoy.jwt metadata, which is indexed by the issuer.
    // For multiple JWT, we only select on of them, the first one lexically
    // sorted.
    std::string issuer_selected = "";
    for (const auto& entry : *filter_it) {
      const std::string& issuer = entry.first;
      if (issuer_selected == "" || issuer_selected.compare(issuer)) {
        issuer_selected = issuer;
      }
    }
    ::google::protobuf::Struct data = (*filter_it)[issuer_selected];
  }

  ENVOY_LOG(info, "Saved Dynamic Metadata:\n{}", auth_attr.DebugString());

  // filter_context_.reset(new Istio::AuthN::FilterContext(
  //     decoder_callbacks_->streamInfo().dynamicMetadata(), headers,
  //     decoder_callbacks_->connection(), filter_config_));

  // Payload payload;

  // if (!createPeerAuthenticator(filter_context_.get())->run(&payload) &&
  //     !filter_config_.policy().peer_is_optional()) {
  //   rejectRequest("Peer authentication failed.");
  //   return FilterHeadersStatus::StopIteration;
  // }

  // bool success =
  //     createOriginAuthenticator(filter_context_.get())->run(&payload) ||
  //     filter_config_.policy().origin_is_optional();

  // if (!success) {
  //   rejectRequest("Origin authentication failed.");
  //   return FilterHeadersStatus::StopIteration;
  // }

  // // Put authentication result to headers.
  // if (filter_context_ != nullptr) {
  //   // Save auth results in the metadata, could be used later by RBAC and/or
  //   // mixer filter.
  //   ProtobufWkt::Struct data;
  //   Utils::Authentication::SaveAuthAttributesToStruct(
  //       filter_context_->authenticationResult(), data);
  //   decoder_callbacks_->streamInfo().setDynamicMetadata(
  //       Utils::IstioFilterName::kAuthentication, data);
  //   ENVOY_LOG(debug, "Saved Dynamic Metadata:\n{}", data.DebugString());
  // }
  // state_ = State::COMPLETE;
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AuthenticationFilter::decodeData(Buffer::Instance&, bool) {
  // TODO(incfly): what's this for? why
  if (state_ == State::PROCESSING) {
    return FilterDataStatus::StopIterationAndWatermark;
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus AuthenticationFilter::decodeTrailers(HeaderMap&) {
  if (state_ == State::PROCESSING) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void AuthenticationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
