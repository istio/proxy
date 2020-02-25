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
	"fmt"
	"testing"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const outboundStatsFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.metadata_exchange" }
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stats_outbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          local: { inline_string: "envoy.wasm.stats" }
      configuration: |
        { "debug": "false", max_peer_cache_size: 20, field_separator: ";.;", "disable_host_header_fallback": %t}`

const inboundStatsFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.metadata_exchange" }
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stats_inbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          local: { inline_string: "envoy.wasm.stats" }
      configuration: |
        { "debug": "false", max_peer_cache_size: 20, field_separator: ";.;"}`

var (
	statsConfig          = driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl")
	outboundNodeMetadata = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	inboundNodeMetadata  = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
)

// Stats in Server Envoy proxy.
var expectedPrometheusServerStats = map[string]env.Stat{
	"istio_requests_total": {Value: 10,
		Labels: map[string]string{
			"grpc_response_status":           "",
			"destination_canonical_service":  "ratings",
			"destination_canonical_revision": "version-1",
			"source_canonical_service":       "productpage-v1",
			"source_canonical_revision":      "version-1",
		}},
	"istio_build": {Value: 1},
}

func TestStatsPlugin(t *testing.T) {
	testStatsPlugin(t, true, func(s *env.TestSetup) {
		s.VerifyPrometheusStats(expectedPrometheusServerStats, s.Ports().ServerAdminPort)
		clntStats := map[string]env.Stat{
			"istio_requests_total": {Value: 10, Labels: map[string]string{
				"destination_service":            "unknown",
				"grpc_response_status":           "",
				"destination_canonical_service":  "ratings",
				"destination_canonical_revision": "version-1",
				"source_canonical_service":       "productpage-v1",
				"source_canonical_revision":      "version-1",
			}},
			"istio_build": {Value: 1},
		}
		s.VerifyPrometheusStats(clntStats, s.Ports().ClientAdminPort)
	})
}

func TestStatsPluginHHFallback(t *testing.T) {
	testStatsPlugin(t, false, func(s *env.TestSetup) {
		s.VerifyPrometheusStats(expectedPrometheusServerStats, s.Ports().ServerAdminPort)
		clntStats := map[string]env.Stat{
			"istio_requests_total": {Value: 10, Labels: map[string]string{
				"destination_service":            fmt.Sprintf("127.0.0.1:%d", s.Ports().AppToClientProxyPort),
				"destination_canonical_service":  "ratings",
				"destination_canonical_revision": "version-1",
				"source_canonical_service":       "productpage-v1",
				"source_canonical_revision":      "version-1",
			}},
			"istio_build": {Value: 1},
		}
		s.VerifyPrometheusStats(clntStats, s.Ports().ClientAdminPort)
	})
}

type verifyFn func(s *env.TestSetup)

func testStatsPlugin(t *testing.T, disableHostHeaderFallback bool, fn verifyFn) {
	s := env.NewClientServerEnvoyTestSetup(env.StatsPluginTest, t)
	s.SetFiltersBeforeEnvoyRouterInAppToClient(fmt.Sprintf(outboundStatsFilter, disableHostHeaderFallback))
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(inboundStatsFilter)
	s.SetServerNodeMetadata(inboundNodeMetadata)
	s.SetClientNodeMetadata(outboundNodeMetadata)
	s.SetExtraConfig(statsConfig)
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	url := fmt.Sprintf("http://127.0.0.1:%d/echo", s.Ports().AppToClientProxyPort)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	for i := 0; i < 10; i++ {
		if _, _, err := env.HTTPGet(url); err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
	}
	fn(s)
}
