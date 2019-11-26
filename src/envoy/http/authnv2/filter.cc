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

#include "src/envoy/http/authnv2/filter.h"

#include "common/http/utility.h"
#include "extensions/filters/http/well_known_names.h"
#include "include/istio/utils/attribute_names.h"
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
  }
  if (filter_it != metadata.filter_metadata().end()) {
    const ::google::protobuf::Struct& jwt_metadata = filter_it->second;
    // Iterate over the envoy.jwt metadata, which is indexed by the issuer.
    // For multiple JWT, we only select on of them, the first one lexically
    // sorted.
    std::string issuer_selected = "";
    for (const auto& entry : jwt_metadata.fields()) {
      const std::string& issuer = entry.first;
      if (issuer_selected == "" || issuer_selected.compare(issuer)) {
        issuer_selected = issuer;
      }
    }
    std::string jwt_payload;
    const auto& jwt_entry = jwt_metadata.fields().find(issuer_selected);
    Protobuf::util::MessageToJsonString(jwt_entry->second.struct_value(),
                                        &jwt_payload);
    // Handling the attributes for request.auth.
    setKeyValueFoo(auth_attr,
                   istio::utils::AttributeName::kRequestAuthPrincipal, "foo");
    ENVOY_LOG(info, "jwt metadata {} \njwt payload selected {}, issuer {}",
              metadata.DebugString(), jwt_payload, issuer_selected);
  }

  ENVOY_LOG(info, "Saved Dynamic Metadata:\n{}", auth_attr.DebugString());
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AuthenticationFilter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus AuthenticationFilter::decodeTrailers(HeaderMap&) {
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
