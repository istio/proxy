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

#include "source/extensions/filters/network/forward_downstream_sni/forward_downstream_sni.h"

#include "envoy/network/connection.h"
#include "source/common/network/upstream_server_name.h"

namespace Envoy {
namespace Tcp {
namespace ForwardDownstreamSni {

using ::Envoy::Network::UpstreamServerName;

Network::FilterStatus ForwardDownstreamSniFilter::onNewConnection() {
  absl::string_view sni = read_callbacks_->connection().requestedServerName();

  if (!sni.empty()) {
    read_callbacks_->connection().streamInfo().filterState()->setData(
        UpstreamServerName::key(), std::make_unique<UpstreamServerName>(sni),
        StreamInfo::FilterState::StateType::ReadOnly);
  }

  return Network::FilterStatus::Continue;
}

} // namespace ForwardDownstreamSni
} // namespace Tcp
} // namespace Envoy
