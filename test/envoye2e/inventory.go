// Copyright 2020 Istio Authors
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

package envoye2e

import (
	"istio.io/proxy/test/envoye2e/env"
)

var ProxyE2ETests *env.TestInventory

func init() {
	// TODO(bianpengyuan): automatically generate this.
	ProxyE2ETests = &env.TestInventory{
		Tests: []string{
			"TestAttributeGen",
			"TestBasicFlow",
			"TestBasicHTTP",
			"TestBasicHTTPGateway",
			"TestBasicHTTPwithTLS",
			"TestBasicTCPFlow",
			"TestBasicCONNECT/quic",
			"TestBasicCONNECT/h2",
			"TestPassthroughCONNECT/quic",
			"TestPassthroughCONNECT/h2",
			"TestHTTPExchange",
			"TestNativeHTTPExchange",
			"TestHTTPLocalRatelimit",
			"TestStackdriverAccessLog/AllClientErrorRequestsGetsLoggedOnNoMxAndError",
			"TestStackdriverAccessLog/AllErrorRequestsGetsLogged",
			"TestStackdriverAccessLog/NoClientRequestsGetsLoggedOnErrorConfigAndAllSuccessRequests",
			"TestStackdriverAccessLog/RequestGetsLoggedAgain",
			"TestStackdriverAccessLog/StackdriverAndAccessLogPlugin",
			"TestStackdriverAccessLogFilter",
			"TestStackdriverAttributeGen",
			"TestStackdriverAuditLog",
			"TestStackdriverCustomAccessLog",
			"TestStackdriverGCEInstances",
			"TestStackdriverGenericNode",
			"TestStackdriverMetricExpiry",
			"TestStackdriverParallel",
			"TestStackdriverPayload",
			"TestStackdriverPayloadGateway",
			"TestStackdriverPayloadUtf8",
			"TestStackdriverPayloadWithTLS",
			"TestStackdriverRbacAccessDenied/ActionAllow",
			"TestStackdriverRbacAccessDenied/ActionBoth",
			"TestStackdriverRbacAccessDenied/ActionDeny",
			"TestStackdriverRbacTCPDryRun",
			"TestStackdriverRbacTCPDryRun/BaseCase",
			"TestStackdriverRbacTCPDryRun/NoAlpn",
			"TestStackdriverReload",
			"TestStackdriverTCPMetadataExchange/BaseCase",
			"TestStackdriverTCPMetadataExchange/NoAlpn",
			"TestStackdriverVMReload",
			"TestStats403Failure/envoy.wasm.runtime.v8",
			"TestStats403Failure/#00",
			"TestStatsECDS/envoy.wasm.runtime.v8",
			"TestStatsECDS/#00",
			"TestStatsEndpointLabels/envoy.wasm.runtime.v8",
			"TestStatsEndpointLabels/#00",
			"TestStatsServerWaypointProxy",
			"TestStatsServerWaypointProxyCONNECT",
			"TestStatsGrpc/envoy.wasm.runtime.v8",
			"TestStatsGrpc/#00",
			"TestStatsGrpcStream/envoy.wasm.runtime.v8",
			"TestStatsGrpcStream/#00",
			"TestStatsParallel/Default",
			"TestStatsParallel/Customized",
			"TestStatsPayload/Customized/envoy.wasm.runtime.v8",
			"TestStatsPayload/Customized/",
			"TestStatsPayload/Default/envoy.wasm.runtime.v8",
			"TestStatsPayload/Default/",
			"TestStatsPayload/DisableHostHeader/envoy.wasm.runtime.v8",
			"TestStatsPayload/DisableHostHeader/",
			"TestStatsPayload/UseHostHeader/envoy.wasm.runtime.v8",
			"TestStatsPayload/UseHostHeader/",
			"TestStatsParserRegression",
			"TestStatsExpiry",
			"TestTCPMetadataExchange/false",
			"TestTCPMetadataExchange/true",
			"TestTCPMetadataExchangeNoAlpn",
			"TestTCPMetadataExchangeWithConnectionTermination",
			"TestOtelPayload",
		},
	}
}
