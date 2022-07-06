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
	env "istio.io/proxy/test/envoye2e/env"
)

var ProxyE2ETests *env.TestInventory

func init() {
	// TODO(bianpengyuan): automatically generate this.
	ProxyE2ETests = &env.TestInventory{
		Tests: []string{
			"TestAttributeGen/envoy.wasm.runtime.null",
			"TestAttributeGen/envoy.wasm.runtime.v8",
			"TestBasicFlow",
			"TestBasicHTTP",
			"TestBasicHTTPGateway",
			"TestBasicHTTPwithTLS",
			"TestBasicTCPFlow",
			"TestHTTPExchange",
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
			"TestStats403Failure/envoy.wasm.runtime.null",
			"TestStats403Failure/envoy.wasm.runtime.v8",
			"TestStats403Failure/envoy.wasm.runtime.v8#01",
			"TestStatsECDS/envoy.wasm.runtime.null",
			"TestStatsECDS/envoy.wasm.runtime.v8",
			"TestStatsECDS/envoy.wasm.runtime.v8#01",
			"TestStatsEndpointLabels",
			"TestStatsGrpc",
			"TestStatsGrpcStream",
			"TestStatsParallel/Default",
			"TestStatsParallel/Customized",
			"TestStatsPayload/Customized/envoy.wasm.runtime.null",
			"TestStatsPayload/Customized/envoy.wasm.runtime.v8",
			"TestStatsPayload/Customized/envoy.wasm.runtime.v8#01",
			"TestStatsPayload/Default/envoy.wasm.runtime.null",
			"TestStatsPayload/Default/envoy.wasm.runtime.v8",
			"TestStatsPayload/Default/envoy.wasm.runtime.v8#01",
			"TestStatsPayload/DisableHostHeader/envoy.wasm.runtime.null",
			"TestStatsPayload/DisableHostHeader/envoy.wasm.runtime.v8",
			"TestStatsPayload/DisableHostHeader/envoy.wasm.runtime.v8#01",
			"TestStatsPayload/UseHostHeader/envoy.wasm.runtime.null",
			"TestStatsPayload/UseHostHeader/envoy.wasm.runtime.v8",
			"TestStatsPayload/UseHostHeader/envoy.wasm.runtime.v8#01",
			"TestStatsParserRegression",
			"TestTCPMetadataExchange",
			"TestTCPMetadataExchangeNoAlpn",
		},
	}
}
