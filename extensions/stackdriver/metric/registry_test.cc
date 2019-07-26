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

#include "extensions/stackdriver/metric/registry.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

TEST(RegistryTest, getStackdriverOptions) {
  stackdriver::common::NodeInfo node_info;
  node_info.mutable_platform_metadata()->set_gcp_project("test_project");
  auto option = getStackdriverOptions(node_info);
  EXPECT_EQ(option.project_id, "test_project");
}

// TODO: add more test once https://github.com/envoyproxy/envoy/pull/7622
// reaches istio/proxy.

}  // namespace Metric
}  // namespace Stackdriver
}  // namespace Extensions
