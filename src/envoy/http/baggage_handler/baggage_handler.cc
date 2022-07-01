/* Copyright Istio Authors. All Rights Reserved.
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

#include "src/envoy/http/baggage_handler/baggage_handler.h"

#include "source/common/http/header_utility.h"
#include "src/envoy/common/metadata_object.h"
#include "src/envoy/http/baggage_handler/config/baggage_handler.pb.h"

namespace Envoy {
namespace Http {
namespace BaggageHandler {

Config::Config(const istio::telemetry::baggagehandler::v1::Config& proto_config)
    : filter_state_key_(proto_config.filter_state_key()) {}

// Extract the value of the header.
absl::optional<std::string> extract(Http::HeaderMap& map,
                                    absl::string_view key) {
  const auto header_value =
      Http::HeaderUtility::getAllOfHeaderAsString(map, LowerCaseString(key));
  if (!header_value.result().has_value()) {
    return absl::nullopt;
  }
  absl::optional<std::string> value =
      std::string(header_value.result().value());
  return value;
}

Http::FilterHeadersStatus BaggageHandlerFilter::decodeHeaders(
    Http::RequestHeaderMap& headers, bool) {
  absl::optional<std::string> value_optional = extract(headers, "baggage");

  if (value_optional) {
    auto baggage_value = value_optional.value();

    Common::WorkloadMetadataObject sourceMeta =
        Common::WorkloadMetadataObject::fromBaggage(baggage_value);

    sourceMeta.setSsl(decoder_callbacks_->connection()->ssl());

    auto filter_state = decoder_callbacks_->streamInfo().filterState();
    filter_state->setData(
        config_->filterStateKey(),
        std::make_shared<Common::WorkloadMetadataObject>(sourceMeta),
        StreamInfo::FilterState::StateType::ReadOnly,
        StreamInfo::FilterState::LifeSpan::Connection);
  } else {
    ENVOY_LOG(debug, "no baggage header found.");
  }
  return Http::FilterHeadersStatus::Continue;
}

void BaggageHandlerFilter::setDecoderFilterCallbacks(
    Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

Http::FilterHeadersStatus BaggageHandlerFilter::encodeHeaders(
    Http::ResponseHeaderMap&, bool) {
  return Http::FilterHeadersStatus::Continue;
}

void BaggageHandlerFilter::setEncoderFilterCallbacks(
    Http::StreamEncoderFilterCallbacks& callbacks) {
  encoder_callbacks_ = &callbacks;
}

}  // namespace BaggageHandler
}  // namespace Http
}  // namespace Envoy
