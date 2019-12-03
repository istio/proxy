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

#include <string>

#include "common/common/logger.h"
#include "common/http/utility.h"
#include "extensions/filters/http/well_known_names.h"

#include "src/envoy/http/authnv2/filter.h"
#include "src/envoy/utils/authn.h"
#include "src/envoy/utils/filter_names.h"
#include "src/envoy/utils/utils.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"

#include "common/json/json_loader.h"
#include "google/protobuf/struct.pb.h"
#include "include/istio/utils/attribute_names.h"
#include "src/envoy/http/jwt_auth/jwt.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

namespace {

// The JWT audience key name
static const std::string kJwtAudienceKey = "aud";
// The JWT issuer key name
static const std::string kJwtIssuerKey = "iss";
// The key name for the original claims in an exchanged token
static const std::string kExchangedTokenOriginalPayload = "original_claims";

// Extract JWT claim as a string list.
// This function only extracts string and string list claims.
// A string claim is extracted as a string list of 1 item.
// A string claim with whitespace is extracted as a string list with each
// sub-string delimited with the whitespace.
// void ExtractStringList(const std::string& key, const Envoy::Json::Object&
// obj,
//                        std::vector<std::string>& list) {
//   // First, try as string
//   try {
//     // Try as string, will throw execption if object type is not string.
//     const std::vector<std::string> keys =
//         absl::StrSplit(obj.getString(key), ' ', absl::SkipEmpty());
//     for (const auto& key : keys) {
//       list.push_back(key);
//     }
//     return;
//   } catch (Json::Exception& e) {
//     // Not convertable to string
//   }
//   // Next, try as string array
//   try {
//     std::vector<std::string> vector = obj.getStringArray(key);
//     for (const std::string v : vector) {
//       list.push_back(v);
//     }
//   } catch (Json::Exception& e) {
//     // Not convertable to string array
//   }
// }

// Helper function to set a key/value pair into Struct.
void setKeyValue(ProtobufWkt::Struct& data, const std::string& key,
                 const std::string& value) {
  (*data.mutable_fields())[key].set_string_value(value);
}

// Returns true if the peer identity can be extracted from the underying
// certificate used for mTLS.
bool ProcessMtls(const Network::Connection* connection,
                 ProtobufWkt::Struct& authn_data) {
  std::string peer_principle;
  if (connection->ssl() == nullptr ||
      !connection->ssl()->peerCertificatePresented()) {
    return false;
  }
  Utils::GetPrincipal(connection, true, &peer_principle);
  if (peer_principle == "") {
    return false;
  }
  setKeyValue(authn_data, istio::utils::AttributeName::kSourcePrincipal,
              peer_principle);
  return true;
}

const std::string getClaimValue(const ProtobufWkt::Struct& claim_structs,
                                const std::string& key) {
  const auto& claim_fields = claim_structs.fields();
  if (claim_fields.find(key) == claim_fields.end()) {
    return "";
  }
  // Try string_value first.
  const auto& value = claim_fields.at(key);
  const std::string& str_val = value.string_value();
  if (str_val != "") {
    return str_val;
  }
  // Try list_string second.
  const auto& values = value.list_value().values();
  if (!values.empty()) {
    return values[0].string_value();
  }
  return "";
}

}  // namespace

// Returns true if the attribute populated to authn filter succeeds.
// Envoy jwt filter already set each claim values into the struct.
// https://github.com/envoyproxy/envoy/blob/master/source/extensions/filters/http/jwt_authn/verifier.cc#L120
bool AuthnV2Filter::processJwt(const std::string& jwt,
                               const ProtobufWkt::Struct& claim_structs,
                               ProtobufWkt::Struct& authn_data) {
  ENVOY_LOG(info, "abc {}\n{}\n{}", claim_structs.DebugString(), jwt,
            authn_data.DebugString());
  // Envoy::Json::ObjectSharedPtr json_obj;
  // try {
  //   json_obj = Json::Factory::loadFromString(jwt);
  // } catch (...) {
  //   return false;
  // }

  // ProtobufWkt::Struct claim_structs;
  // auto claims = &claim_structs.fields();
  // // Extract claims as string lists
  // // json_obj->iterate([json_obj, claims](const std::string& key,
  // //                                      const Json::Object&) -> bool {
  // //   // In current implementation, only string/string list objects are
  // //   extracted std::vector<std::string> list; ExtractStringList(key,
  // //   *json_obj, list); for (auto s : list) {
  // // claims->at(key].mutable_list_value()->add_values()->set_string_value(s);
  // //   }
  // //   return true;
  // // });
  const std::string aud = getClaimValue(claim_structs, kJwtAudienceKey);
  if (!aud.empty()) {
    setKeyValue(authn_data, istio::utils::AttributeName::kRequestAuthAudiences,
                aud);
  }

  // if (claims->find(kJwtAudienceKey) != claims->end()) {
  //   // TODO(diemtvu): this should be send as repeated field once mixer
  //   // support string_list (https://github.com/istio/istio/issues/2802) For
  //   // now, just use the first value.
  //   const auto& aud_values =
  //   claims->at(kJwtAudienceKey).list_value().values(); if
  //   (!aud_values.empty()) {
  //     setKeyValue(authn_data,
  //                 istio::utils::AttributeName::kRequestAuthAudiences,
  //                 aud_values[0].string_value());
  //   }
  // }

  // // request.auth.principal
  // if (claims->find("iss") != claims->end() &&
  //     claims->find("sub") != claims->end()) {
  //   setKeyValue(
  //       authn_data, istio::utils::AttributeName::kRequestAuthPrincipal,
  //       claims->at("iss").list_value().values().Get(0).string_value() + "/" +
  //           claims->at("sub").list_value().values().Get(0).string_value());
  // }

  const std::string iss = getClaimValue(claim_structs, "iss");
  const std::string sub = getClaimValue(claim_structs, "sub");
  if (!iss.empty() && !sub.empty()) {
    setKeyValue(authn_data, istio::utils::AttributeName::kRequestAuthPrincipal,
                iss + "/" + sub);
  }

  const std::string azp = getClaimValue(claim_structs, "azp");
  if (!azp.empty()) {
    setKeyValue(authn_data, istio::utils::AttributeName::kRequestAuthPresenter,
                azp);
  }
  // // request.auth.audiences
  // if (claims->find("azp") != claims->end()) {
  //   setKeyValue(authn_data,
  //   istio::utils::AttributeName::kRequestAuthPresenter,
  //               claims->at("azp").list_value().values().Get(0).string_value());
  // }

  // request.auth.claims
  (*(*authn_data
          .mutable_fields())[istio::utils::AttributeName::kRequestAuthClaims]
        .mutable_struct_value())
      .MergeFrom(claim_structs);

  // request.auth.raw_claims
  setKeyValue(authn_data, istio::utils::AttributeName::kRequestAuthRawClaims,
              jwt);
  return true;
}

void AuthnV2Filter::onDestroy() {
  ENVOY_LOG(debug, "Called AuthnV2Filter : {}", __func__);
}

std::pair<std::string, const ProtobufWkt::Struct*>
AuthnV2Filter::extractJwtFromMetadata(
    const envoy::api::v2::core::Metadata& metadata, std::string* jwt_payload) {
  auto result = std::pair<std::string, const ProtobufWkt::Struct*>("", nullptr);
  std::string issuer_selected = "";
  auto filter_it = metadata.filter_metadata().find(
      Extensions::HttpFilters::HttpFilterNames::get().JwtAuthn);
  if (filter_it == metadata.filter_metadata().end()) {
    return result;
  }
  const ProtobufWkt::Struct& jwt_metadata = filter_it->second;
  // Iterate over the envoy.jwt metadata, which is indexed by the issuer.
  // For multiple JWT, we only select on of them, the first one lexically
  // sorted.
  for (const auto& entry : jwt_metadata.fields()) {
    const std::string& issuer = entry.first;
    if (issuer_selected == "" || issuer_selected.compare(issuer) > 0) {
      issuer_selected = issuer;
    }
  }
  if (issuer_selected != "") {
    const auto& jwt_entry = jwt_metadata.fields().find(issuer_selected);
    result.first = issuer_selected;
    result.second = &(jwt_entry->second.struct_value());
    Protobuf::util::MessageToJsonString(jwt_entry->second.struct_value(),
                                        jwt_payload);
  }
  return result;
}

FilterHeadersStatus AuthnV2Filter::decodeHeaders(HeaderMap&, bool) {
  ENVOY_LOG(debug, "AuthnV2Filter::decodeHeaders start\n");
  auto& metadata = decoder_callbacks_->streamInfo().dynamicMetadata();
  auto& authn_data =
      (*metadata.mutable_filter_metadata())[Utils::IstioFilterName::kAuthnV2];

  // Always try to get principal and set to output if available.
  if (!ProcessMtls(decoder_callbacks_->connection(), authn_data)) {
    ENVOY_LOG(warn, "unable to extract peer identity");
  }
  std::string jwt_payload = "";
  auto result = extractJwtFromMetadata(metadata, &jwt_payload);
  ENVOY_LOG(debug,
            "extract jwt metadata {} \njwt payload issuer {}, payload\n{}\n",
            metadata.DebugString(), result.first, jwt_payload);
  if (result.first != "") {
    // const auto& jwt_struct =
    processJwt(jwt_payload, *result.second, authn_data);
  }
  ENVOY_LOG(debug, "Saved Dynamic Metadata:\n{}",
            Envoy::MessageUtil::getYamlStringFromMessage(
                decoder_callbacks_->streamInfo()
                    .dynamicMetadata()
                    .filter_metadata()
                    .at(Utils::IstioFilterName::kAuthnV2),
                true, true));
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AuthnV2Filter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus AuthnV2Filter::decodeTrailers(HeaderMap&) {
  return FilterTrailersStatus::Continue;
}

void AuthnV2Filter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
