// Copyright 2019 Istio Authors
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

package client_test

import (
	"bytes"
	"fmt"
	"testing"

	"text/template"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const metadataExchangeIstioDownstreamConfigFilter = `- name: envoy.filters.network.metadata_exchange
  config:
    protocol: istio2`

const metadataExchangeIstioStatsServerFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stats_inbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          local: { inline_string: "envoy.wasm.stats" }
      configuration: |
        { "debug": false, "field_separator": ";.;" }`
const metadataExchangeIstioUpstreamConfigFilterChain = `filters:
- name: envoy.filters.network.upstream.metadata_exchange
  typed_config: 
    "@type": type.googleapis.com/envoy.tcp.metadataexchange.config.MetadataExchange
    protocol: istio2`

const metadataExchangeIstioStatsClientFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stats_outbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          local: { inline_string: "envoy.wasm.stats" }
      configuration: |
        { "debug": false, "field_separator": ";.;" }`

const tlsContext = `tls_context:
  common_tls_context:
    alpn_protocols:
    - istio2
    tls_certificates:
    - certificate_chain: { filename: "testdata/certs/cert-chain.pem" }
      private_key: { filename: "testdata/certs/key.pem" }
    validation_context:
      trusted_ca: { filename: "testdata/certs/root-cert.pem" }
  require_client_certificate: true`

const clusterTLSContext = `tls_context:
  common_tls_context:
    alpn_protocols:
    - istio2
    tls_certificates:
    - certificate_chain: { filename: "testdata/certs/cert-chain.pem" }
      private_key: { filename: "testdata/certs/key.pem" }
    validation_context:
      trusted_ca: { filename: "testdata/certs/root-cert.pem" }`

var (
	statsConfig          = driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl")
	outboundNodeMetadata = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	inboundNodeMetadata  = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
)

// Stats in Client Envoy proxy.
var expectedClientStats = map[string]int{
	"cluster.client.metadata_exchange.alpn_protocol_found":      1,
	"cluster.client.metadata_exchange.alpn_protocol_not_found":  0,
	"cluster.client.metadata_exchange.initial_header_not_found": 0,
	"cluster.client.metadata_exchange.header_not_found":         0,
	"cluster.client.metadata_exchange.metadata_added":           1,
}

// Stats in Server Envoy proxy.
var expectedServerStats = map[string]int{
	"metadata_exchange.alpn_protocol_found":      1,
	"metadata_exchange.alpn_protocol_not_found":  0,
	"metadata_exchange.initial_header_not_found": 0,
	"metadata_exchange.header_not_found":         0,
	"metadata_exchange.metadata_added":           1,
}

func TestHttpMetadataExchange(t *testing.T) {
	t.Skip("Enable after https://github.com/envoyproxy/envoy-wasm/issues/402  is fixed")
	testPlugins(t, func(s *env.TestSetup) {
		serverStats := map[string]env.Stat{
			"istio_requests_total": {Value: 10, Labels: map[string]string{"destination_service": "server.default.svc.cluster.local",
				"source_app": "productpage", "destination_app": "ratings"}},
		}
		s.VerifyPrometheusStats(serverStats, s.Ports().ServerAdminPort)
		s.VerifyEnvoyStats(getParsedExpectedStats(expectedClientStats, t, s), s.Ports().ClientAdminPort)
		s.VerifyEnvoyStats(getParsedExpectedStats(expectedServerStats, t, s), s.Ports().ServerAdminPort)
	})
}

type verifyFn func(s *env.TestSetup)

func testPlugins(t *testing.T, fn verifyFn) {
	s := env.NewClientServerEnvoyTestSetup(env.HTTPMetadataExchangeTest, t)
	s.Dir = driver.BazelWorkspace()
	s.SetTLSContext(tlsContext)
	s.SetClusterTLSContext(clusterTLSContext)
	s.SetUpstreamFiltersInClient(metadataExchangeIstioUpstreamConfigFilterChain)
	s.SeFiltersBeforeHTTPConnectionManagerInProxyToServer(metadataExchangeIstioDownstreamConfigFilter)
	s.SetFiltersBeforeEnvoyRouterInAppToClient(metadataExchangeIstioStatsClientFilter)
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(metadataExchangeIstioStatsServerFilter)
	s.SetServerNodeMetadata(inboundNodeMetadata)
	s.SetClientNodeMetadata(outboundNodeMetadata)
	s.SetExtraConfig(statsConfig)
	s.SetEnableTLS(true)
	s.SetCopyYamlFiles(true)
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	url := fmt.Sprintf("https://127.0.0.1:%d/echo", s.Ports().AppToClientProxyPort)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	for i := 0; i < 10; i++ {
		if _, _, err := env.HTTPTlsGet(url, s.Dir, s.Ports().AppToClientProxyPort); err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
	}
	fn(s)
}

func getParsedExpectedStats(expectedStats map[string]int, t *testing.T, s *env.TestSetup) map[string]int {
	parsedExpectedStats := make(map[string]int)
	for key, value := range expectedStats {
		tmpl, err := template.New("parse_state").Parse(key)
		if err != nil {
			t.Errorf("failed to parse config template: %v", err)
		}

		var tpl bytes.Buffer
		err = tmpl.Execute(&tpl, s)
		if err != nil {
			t.Errorf("failed to execute config template: %v", err)
		}
		parsedExpectedStats[tpl.String()] = value
	}

	return parsedExpectedStats
}
