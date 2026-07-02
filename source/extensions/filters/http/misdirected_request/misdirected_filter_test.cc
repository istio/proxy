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

#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "envoy/config/core/v3/base.pb.h"
#include "gtest/gtest.h"
#include "source/extensions/filters/http/misdirected_request/misdirected_filter.h"

namespace Envoy {
namespace Http {
namespace MisdirectedRequest {
namespace {

// The four HTTPS listeners sharing port 443 in the Gateway API conformance test
// "HTTPRouteHTTPSListenerDetectMisdirectedRequests": a catch-all listener, an
// exact listener, a wildcard listener, and an exact listener nested under the
// wildcard.
const std::vector<std::string>& portHostnames() {
  static const std::vector<std::string> hosts = {"*", "second-example.org", "*.wildcard.org",
                                                  "fourth-example.wildcard.org"};
  return hosts;
}

// Builds listener metadata carrying the full per-port hostname set under the
// default namespace/field the filter reads.
envoy::config::core::v3::Metadata listenerMetadata() {
  envoy::config::core::v3::Metadata md;
  auto& ns = (*md.mutable_filter_metadata())["istio.misdirected_request"];
  auto* list = (*ns.mutable_fields())["hosts"].mutable_list_value();
  for (const auto& h : portHostnames()) {
    list->add_values()->set_string_value(h);
  }
  return md;
}

MisdirectedFilterConfig configForOwn(const std::vector<std::string>& own_hostnames) {
  ProtoFilterConfig proto;
  for (const auto& h : own_hostnames) {
    proto.add_own_hostnames(h);
  }
  return MisdirectedFilterConfig(proto, listenerMetadata());
}

// Each case is (selected listener's own hostnames, request host, expect 421).
// "expect 421 == false" means the filter lets routing proceed (which the
// conformance suite then resolves to 200 or 404 — outside this filter's scope).
struct Case {
  std::vector<std::string> own;
  std::string host;
  bool expect_misdirected;
};

TEST(MisdirectedFilterConfigTest, ConformanceScenarios) {
  const std::vector<std::string> catch_all = {"*"};
  const std::vector<std::string> second = {"second-example.org"};
  const std::vector<std::string> wildcard = {"*.wildcard.org"};
  const std::vector<std::string> fourth = {"fourth-example.wildcard.org"};

  const std::vector<Case> cases = {
      // SNI=example.org -> catch-all listener selected.
      {catch_all, "example.org", false},        // 200
      {catch_all, "second-example.org", true},  // 421
      {catch_all, "unknown-example.org", false}, // 404 via routing
      // SNI=second-example.org -> exact listener selected.
      {second, "second-example.org", false},    // 200
      {second, "example.org", true},            // 421
      {second, "unknown-example.org", true},    // 421 (catch-all listener matches)
      // SNI=third-example.wildcard.org -> wildcard listener selected.
      {wildcard, "third-example.wildcard.org", false}, // 200
      {wildcard, "fith-example.wildcard.org", false},  // 200
      {wildcard, "fourth-example.wildcard.org", true}, // 421 (nested exact sibling)
      {wildcard, "second-example.org", true},          // 421
      {wildcard, "unknown-example.org", true},         // 421 (catch-all listener matches)
      // SNI=fourth-example.wildcard.org -> nested exact listener selected.
      {fourth, "fourth-example.wildcard.org", false}, // 200
      {fourth, "fith-example.wildcard.org", true},    // 421 (wildcard sibling)
      // SNI=unknown-example.org -> catch-all listener selected.
      {catch_all, "example.org", false},         // 200
      {catch_all, "unknown-example.org", false}, // 404 via routing
  };

  for (const auto& c : cases) {
    const auto cfg = configForOwn(c.own);
    EXPECT_EQ(c.expect_misdirected, cfg.isMisdirected(c.host))
        << "own=[" << absl::StrJoin(c.own, ",") << "] host=" << c.host;
  }
}

} // namespace
} // namespace MisdirectedRequest
} // namespace Http
} // namespace Envoy
