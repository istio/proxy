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

#include "extensions/stackdriver/common/utils.h"

#include "extensions/stackdriver/common/constants.h"

namespace Extensions {
namespace Stackdriver {
namespace Common {

using google::api::MonitoredResource;

void getMonitoredResource(const std::string &monitored_resource_type,
                          const ::wasm::common::NodeInfo &local_node_info,
                          MonitoredResource *monitored_resource) {
  if (!monitored_resource) {
    return;
  }

  monitored_resource->set_type(monitored_resource_type);
  auto platform_metadata = local_node_info.platform_metadata();

  (*monitored_resource->mutable_labels())[kProjectIDLabel] =
      platform_metadata[kGCPProjectKey];

  if (monitored_resource_type == kGCEInstanceMonitoredResource) {
    // gce_instance

    (*monitored_resource->mutable_labels())[kGCEInstanceIDLabel] =
        platform_metadata[kGCPGCEInstanceIDKey];
    (*monitored_resource->mutable_labels())[kZoneLabel] =
        platform_metadata[kGCPLocationKey];
  } else {
    // k8s_pod or k8s_container

    (*monitored_resource->mutable_labels())[kLocationLabel] =
        platform_metadata[kGCPLocationKey];
    (*monitored_resource->mutable_labels())[kClusterNameLabel] =
        platform_metadata[kGCPClusterNameKey];
    (*monitored_resource->mutable_labels())[kNamespaceNameLabel] =
        local_node_info.namespace_();
    (*monitored_resource->mutable_labels())[kPodNameLabel] =
        local_node_info.name();

    if (monitored_resource_type == kContainerMonitoredResource) {
      // Fill in container_name of k8s_container monitored resource.
      (*monitored_resource->mutable_labels())[kContainerNameLabel] =
          kIstioProxyContainerName;
    }
  }
}

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions
