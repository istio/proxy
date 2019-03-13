/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include <map>
#include <string>

#include "envoy/http/header_map.h"
#include "envoy/network/connection.h"
#include "src/istio/context/context.pb.h"

namespace istio {
namespace context {

void ExtractHeaders(Request& request, const ::Envoy::Http::HeaderMap& headers);

void ExtractContext(Context& context, const ::Envoy::Http::HeaderMap& headers);

void ExtractConnection(Connection& connection, const ::Envoy::Network::Connection& downstream);

void ExtractOrigin(Origin& origin, const ::Envoy::Network::Connection& downstream);

}  // namespace context
}  // namespace istio
