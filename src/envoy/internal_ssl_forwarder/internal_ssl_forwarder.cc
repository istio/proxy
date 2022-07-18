// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/envoy/internal_ssl_forwarder/internal_ssl_forwarder.h"

#include "envoy/network/connection.h"
#include "envoy/stream_info/filter_state.h"
#include "src/envoy/common/metadata_object.h"
#include "src/envoy/internal_ssl_forwarder/config/internal_ssl_forwarder.pb.h"

using namespace istio::telemetry::internal_ssl_forwarder;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace InternalSslForwarder {

Network::FilterStatus Filter::onNewConnection() {
  auto filter_state = callbacks_->connection().streamInfo().filterState();

  const Common::WorkloadMetadataObject* meta_obj =
      filter_state->getDataReadOnly<Common::WorkloadMetadataObject>(
          Common::WorkloadMetadataObject::kSourceMetadataObjectKey);

  if (meta_obj == nullptr) {
    ENVOY_LOG(trace, "internal_ssl_forwarder: no metadata object found");
    return Network::FilterStatus::Continue;
  }

  callbacks_->connection().connectionInfoSetter().setSslConnection(
      meta_obj->ssl());
  ENVOY_LOG(trace, "internal_ssl_forwarder: connection ssl set");

  return Network::FilterStatus::Continue;
};

}  // namespace InternalSslForwarder
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
