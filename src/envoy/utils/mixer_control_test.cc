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

#include "src/envoy/utils/mixer_control.h"
#include "mixer/v1/config/client/client_config.pb.h"
#include "src/envoy/utils/utils.h"
#include "test/test_common/utility.h"

using Envoy::Utils::Extract;
using Envoy::Utils::NodeKey;
using Envoy::Utils::ParseJsonMessage;
using Envoy::Utils::ReadMap;
using ::istio::utils::AttributeName;
using ::istio::utils::CreateLocalAttributes;
using ::istio::utils::LocalAttributes;
using ::istio::utils::LocalNode;

#define assertEqual(laExpect, la)                                              \
  {                                                                            \
    EXPECT_EQ((laExpect)->outbound.DebugString(),                              \
              (la)->outbound.DebugString());                                   \
    EXPECT_EQ((laExpect)->inbound.DebugString(), (la)->inbound.DebugString()); \
    EXPECT_EQ((laExpect)->forward.DebugString(), (la)->forward.DebugString()); \
  };

namespace {

std::unique_ptr<const LocalAttributes> GenerateLocalAttributes(
    envoy::api::v2::core::Node& node) {
  LocalNode largs;
  if (!Extract(node, &largs)) {
    return nullptr;
  }
  return CreateLocalAttributes(largs);
}

TEST(MixerControlTest, WithMetadata) {
  std::string config_str = R"({
     "id": "NEWID",
     "cluster": "fortioclient",
     "metadata": {
      "ISTIO_VERSION": "1.0.1",
      "NODE_NAME": "fortioclient-84469dc8d7-jbbxt",
      "NODE_IP": "10.36.0.15",
      "NODE_NAMESPACE": "service-graph",
     },
     "build_version": "0/1.8.0-dev//RELEASE"
    })";
  envoy::api::v2::core::Node node;

  auto status = ParseJsonMessage(config_str, &node);
  EXPECT_OK(status) << status;
  std::string val;

  LocalNode largs;
  largs.ip = "10.36.0.15";
  largs.uid = "kubernetes://fortioclient-84469dc8d7-jbbxt.service-graph";
  largs.ns = "service-graph";

  auto la = GenerateLocalAttributes(node);
  EXPECT_NE(la, nullptr);

  const auto att = la->outbound.attributes();

  EXPECT_EQ(true, ReadMap(att, AttributeName::kSourceUID, &val));
  EXPECT_EQ(val, largs.uid);

  EXPECT_EQ(true, ReadMap(att, AttributeName::kSourceNamespace, &val));
  EXPECT_EQ(val, largs.ns);

  auto laExpect = CreateLocalAttributes(largs);

  assertEqual(laExpect, la);
}

TEST(MixerControlTest, NoMetadata) {
  std::string config_str = R"({
     "id": "sidecar~10.36.0.15~fortioclient-84469dc8d7-jbbxt.service-graph~service-graph.svc.cluster.local",
     "cluster": "fortioclient",
     "metadata": {
      "ISTIO_VERSION": "1.0.1",
     },
     "build_version": "0/1.8.0-dev//RELEASE"
    })";
  envoy::api::v2::core::Node node;

  auto status = ParseJsonMessage(config_str, &node);
  EXPECT_OK(status) << status;

  LocalNode largs;
  largs.ip = "10.36.0.15";
  largs.uid = "kubernetes://fortioclient-84469dc8d7-jbbxt.service-graph";
  largs.ns = "service-graph";

  auto la = GenerateLocalAttributes(node);
  EXPECT_NE(la, nullptr);

  const auto att = la->outbound.attributes();
  std::string val;

  EXPECT_EQ(true, ReadMap(att, AttributeName::kSourceUID, &val));
  EXPECT_EQ(val, largs.uid);

  EXPECT_EQ(true, ReadMap(att, AttributeName::kSourceNamespace, &val));
  EXPECT_EQ(val, largs.ns);

  auto laExpect = CreateLocalAttributes(largs);

  assertEqual(laExpect, la);
}
}  // namespace
