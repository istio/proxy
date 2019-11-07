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

#include "include/istio/utils/local_attributes.h"

#include "include/istio/utils/attribute_names.h"
#include "include/istio/utils/attributes_builder.h"

namespace istio {
namespace utils {

namespace {
const char kReporterOutbound[] = "outbound";
}  // namespace

// create Local attributes object and return a pointer to it.
// Should be freed by the caller.
void CreateLocalAttributes(const LocalNode& local,
                           LocalAttributes* local_attributes) {
  ::istio::mixer::v1::Attributes inbound;
  AttributesBuilder ib(&local_attributes->inbound);
  ib.AddString(AttributeName::kDestinationUID, local.uid);
  ib.AddString(AttributeName::kContextReporterUID, local.uid);
  ib.AddString(AttributeName::kDestinationNamespace, local.ns);

  AttributesBuilder ob(&local_attributes->outbound);
  ob.AddString(AttributeName::kSourceUID, local.uid);
  ob.AddString(AttributeName::kContextReporterUID, local.uid);
  ob.AddString(AttributeName::kSourceNamespace, local.ns);

  AttributesBuilder(&local_attributes->forward)
      .AddString(AttributeName::kSourceUID, local.uid);
}

// create preserialized header to send to proxy that is fronting mixer.
// This header is used for istio self monitoring.
bool SerializeForwardedAttributes(const LocalNode& local,
                                  std::string* serialized_forward_attributes) {
  ::istio::mixer::v1::Attributes attributes;
  AttributesBuilder(&attributes)
      .AddString(AttributeName::kSourceUID, local.uid);
  return attributes.SerializeToString(serialized_forward_attributes);
}

// check if this listener is outbound based on "context.reporter.kind" attribute
bool IsOutbound(const ::istio::mixer::v1::Attributes& attributes) {
  bool outbound = false;
  const auto& attributes_map = attributes.attributes();
  const auto it =
      attributes_map.find(::istio::utils::AttributeName::kContextReporterKind);
  if (it != attributes_map.end()) {
    const ::istio::mixer::v1::Attributes_AttributeValue& value = it->second;
    if (kReporterOutbound == value.string_value()) {
      outbound = true;
    }
  }
  return outbound;
}

}  // namespace utils
}  // namespace istio
