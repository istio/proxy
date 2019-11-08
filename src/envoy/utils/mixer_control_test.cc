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

#include "fmt/printf.h"
#include "mixer/v1/config/client/client_config.pb.h"
#include "src/envoy/utils/utils.h"
#include "test/test_common/utility.h"

using Envoy::Utils::ExtractNodeInfo;
using Envoy::Utils::ParseJsonMessage;
using ::istio::utils::AttributeName;
using ::istio::utils::CreateLocalAttributes;
using ::istio::utils::LocalAttributes;
using ::istio::utils::LocalNode;

namespace {

#define ASSERT_LOCAL_NODE(lexp, la)  \
  {                                  \
    EXPECT_EQ((lexp).uid, (la).uid); \
    EXPECT_EQ((lexp).ns, (la).ns);   \
  };

bool ReadAttributeMap(
    const google::protobuf::Map<
        std::string, ::istio::mixer::v1::Attributes_AttributeValue> &meta,
    const std::string &key, std::string *val) {
  const auto it = meta.find(key);
  if (it != meta.end()) {
    *val = it->second.string_value();
    return true;
  }
  return false;
}

const std::string kUID =
    "kubernetes://fortioclient-84469dc8d7-jbbxt.service-graph";
const std::string kNS = "service-graph";
const std::string kNodeID =
    "sidecar~10.36.0.15~fortioclient-84469dc8d7-jbbxt.service-graph~service-"
    "graph.svc.cluster.local";

std::string genNodeConfig(std::string uid, std::string nodeid, std::string ns) {
  auto md = R"(
  "metadata": {
      "ISTIO_VERSION": "1.0.1",
      "NODE_UID": "%s",
      "NODE_NAMESPACE": "%s",
     },
  )";
  std::string meta = "";
  if (!ns.empty()) {
    meta = fmt::sprintf(md, nodeid, ns);
  }

  return fmt::sprintf(R"({
     "id": "%s",
     "cluster": "fortioclient",
     %s
     "build_version": "0/1.8.0-dev//RELEASE"
    })",
                      uid, meta);
}

void initTestLocalNode(LocalNode *lexp) {
  lexp->uid = kUID;
  lexp->ns = kNS;
}

TEST(MixerControlTest, CreateLocalAttributes) {
  LocalNode lexp;
  initTestLocalNode(&lexp);

  LocalAttributes la;
  CreateLocalAttributes(lexp, &la);

  const auto att = la.outbound.attributes();
  std::string val;

  EXPECT_TRUE(ReadAttributeMap(att, AttributeName::kSourceUID, &val));
  EXPECT_TRUE(val == lexp.uid);

  EXPECT_TRUE(ReadAttributeMap(att, AttributeName::kSourceNamespace, &val));
  EXPECT_TRUE(val == lexp.ns);
}

TEST(MixerControlTest, WithMetadata) {
  LocalNode lexp;
  initTestLocalNode(&lexp);

  envoy::api::v2::core::Node node;
  auto status =
      ParseJsonMessage(genNodeConfig("new_id", lexp.uid, lexp.ns), &node);
  EXPECT_OK(status) << status;

  LocalNode largs;
  EXPECT_TRUE(ExtractNodeInfo(node, &largs));

  ASSERT_LOCAL_NODE(lexp, largs);
}

TEST(MixerControlTest, NoMetadata) {
  LocalNode lexp;
  initTestLocalNode(&lexp);

  envoy::api::v2::core::Node node;
  auto status = ParseJsonMessage(genNodeConfig(kNodeID, "", ""), &node);
  EXPECT_OK(status) << status;

  LocalNode largs;
  EXPECT_TRUE(ExtractNodeInfo(node, &largs));

  ASSERT_LOCAL_NODE(lexp, largs);
}

}  // namespace
