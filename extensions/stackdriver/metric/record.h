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

#include "extensions/common/context.h"
#include "extensions/stackdriver/config/v1alpha1/stackdriver_plugin_config.pb.h"
#include "opencensus/stats/tag_key.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

typedef std::vector<std::pair<opencensus::tags::TagKey, std::string>> TagKeyValueList;
typedef std::unordered_map<std::string, TagKeyValueList> override_map;
typedef std::unordered_map<std::string, TagKeyValueList>::value_type override_map_value_type;

// Record metrics based on local node info and request info.
// Reporter kind deceides the type of metrics to record.
void record(bool is_outbound, const ::Wasm::Common::FlatNode& local_node_info,
            const ::Wasm::Common::FlatNode& peer_node_info,
            const ::Wasm::Common::RequestInfo& request_info, bool record_http_size_metrics,
            const override_map& overrides);

// Record TCP metrics based on local node info and request info.
// Reporter kind deceides the type of metrics to record.
void recordTCP(bool is_outbound, const ::Wasm::Common::FlatNode& local_node_info,
               const ::Wasm::Common::FlatNode& peer_node_info,
               const ::Wasm::Common::RequestInfo& request_info, const override_map& overrides);

} // namespace Metric
} // namespace Stackdriver
} // namespace Extensions
