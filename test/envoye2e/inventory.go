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
			"TestBasicFlow",
			"TestBasicHTTP",
			"TestBasicHTTPwithTLS",
			"TestBasicHTTPGateway",
			"TestBasicTCPFlow",
			"TestHTTPExchange",
			"TestStackdriverAccessLog/StackdriverAndAccessLogPlugin",
			"TestStackdriverAccessLog/RequestGetsLoggedAgain",
			"TestStackdriverAccessLog/AllErrorRequestsGetsLogged",
			"TestStackdriverPayload",
			"TestStackdriverPayloadGateway",
			"TestStackdriverPayloadWithTLS",
			"TestStackdriverReload",
			"TestStackdriverVMReload",
			"TestStackdriverParallel",
			"TestStackdriverGCEInstances",
			"TestStackdriverTCPMetadataExchange",
			"TestStackdriverAttributeGen",
			"TestStatsPayload/Default/envoy.wasm.runtime.null",
			"TestStatsPayload/Customized/envoy.wasm.runtime.null",
			"TestStatsPayload/UseHostHeader/envoy.wasm.runtime.null",
			"TestStatsPayload/DisableHostHeader/envoy.wasm.runtime.null",
			"TestStatsPayload/Default/envoy.wasm.runtime.v8",
			"TestStatsPayload/Customized/envoy.wasm.runtime.v8",
			"TestStatsPayload/UseHostHeader/envoy.wasm.runtime.v8",
			"TestStatsPayload/DisableHostHeader/envoy.wasm.runtime.v8",
			"TestStatsParallel",
			"TestStatsGrpc",
			"TestTCPMetadataExchange",
			"TestStackdriverAuditLog",
			"TestTCPMetadataExchangeNoAlpn",
			"TestAttributeGen",
			"TestBasicAuth/CorrectCredentials/envoy.wasm.runtime.null",
			"TestBasicAuth/IncorrectCredentials/envoy.wasm.runtime.null",
			"TestBasicAuth/MissingCredentials/envoy.wasm.runtime.null",
			"TestBasicAuth/NoPathMatch/envoy.wasm.runtime.null",
			"TestBasicAuth/NoMethodMatch/envoy.wasm.runtime.null",
			"TestBasicAuth/NoConfigurationCredentialsProvided/envoy.wasm.runtime.null",
			"TestBasicAuth/CorrectCredentials/envoy.wasm.runtime.v8",
			"TestBasicAuth/IncorrectCredentials/envoy.wasm.runtime.v8",
			"TestBasicAuth/MissingCredentials/envoy.wasm.runtime.v8",
			"TestBasicAuth/NoPathMatch/envoy.wasm.runtime.v8",
			"TestBasicAuth/NoMethodMatch/envoy.wasm.runtime.v8",
			"TestBasicAuth/NoConfigurationCredentialsProvided/envoy.wasm.runtime.v8",
		},
	}
}
