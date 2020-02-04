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

	"istio.io/proxy/test/envoye2e/env"
)

const outboundStatsFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          inline_string: "envoy.wasm.metadata_exchange"
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stats_outbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          inline_string: "envoy.wasm.stats"
      configuration: |
        { "debug": "false", max_peer_cache_size: 20, field_separator: ";.;", "disable_host_header_fallback": %t}`

const inboundStatsFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          inline_string: "envoy.wasm.metadata_exchange"
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stats_inbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          inline_string: "envoy.wasm.stats"
      configuration: |
        { "debug": "false", max_peer_cache_size: 20, field_separator: ";.;"}`

const outboundNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "productpage",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-productpage",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://apis/apps/v1/namespaces/default/deployments/productpage-v1",
"WORKLOAD_NAME": "productpage-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container productpage",
"POD_NAME": "productpage-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "productpage",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "productpage-v1-84975bc778-pxz2w",`

const inboundNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "ratings",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-ratings",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://apis/apps/v1/namespaces/default/deployments/ratings-v1",
"WORKLOAD_NAME": "ratings-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container ratings",
"POD_NAME": "ratings-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "ratings",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "ratings-v1-84975bc778-pxz2w",`

const statsConfig = `stats_config:
  use_all_default_tags: true
  stats_tags:
  - tag_name: "reporter"
    regex: "(reporter=\\.=(.+?);\\.;)"
  - tag_name: "source_namespace"
    regex: "(source_namespace=\\.=(.+?);\\.;)"
  - tag_name: "source_workload"
    regex: "(source_workload=\\.=(.+?);\\.;)"
  - tag_name: "source_workload_namespace"
    regex: "(source_workload_namespace=\\.=(.+?);\\.;)"
  - tag_name: "source_principal"
    regex: "(source_principal=\\.=(.+?);\\.;)"
  - tag_name: "source_app"
    regex: "(source_app=\\.=(.+?);\\.;)"
  - tag_name: "source_version"
    regex: "(source_version=\\.=(.+?);\\.;)"
  - tag_name: "destination_namespace"
    regex: "(destination_namespace=\\.=(.+?);\\.;)"
  - tag_name: "destination_workload"
    regex: "(destination_workload=\\.=(.+?);\\.;)"
  - tag_name: "destination_workload_namespace"
    regex: "(destination_workload_namespace=\\.=(.+?);\\.;)"
  - tag_name: "destination_principal"
    regex: "(destination_principal=\\.=(.+?);\\.;)"
  - tag_name: "destination_app"
    regex: "(destination_app=\\.=(.+?);\\.;)"
  - tag_name: "destination_version"
    regex: "(destination_version=\\.=(.+?);\\.;)"
  - tag_name: "destination_service"
    regex: "(destination_service=\\.=(.+?);\\.;)"
  - tag_name: "destination_service_name"
    regex: "(destination_service_name=\\.=(.+?);\\.;)"
  - tag_name: "destination_service_namespace"
    regex: "(destination_service_namespace=\\.=(.+?);\\.;)"
  - tag_name: "request_protocol"
    regex: "(request_protocol=\\.=(.+?);\\.;)"
  - tag_name: "response_code"
    regex: "(response_code=\\.=(.+?);\\.;)|_rq(_(\\.d{3}))$"
  - tag_name: "response_flags"
    regex: "(response_flags=\\.=(.+?);\\.;)"
  - tag_name: "connection_security_policy"
    regex: "(connection_security_policy=\\.=(.+?);\\.;)"
  - tag_name: "permissive_response_code"
    regex: "(permissive_response_code=\\.=(.+?);\\.;)"
  - tag_name: "permissive_response_policyid"
    regex: "(permissive_response_policyid=\\.=(.+?);\\.;)"
  - tag_name: "cache"
    regex: "(cache\\.(.+?)\\.)"
  - tag_name: "component"
    regex: "(component\\.(.+?)\\.)"
  - tag_name: "tag"
    regex: "(tag\\.(.+?);\\.)"`

// Stats in Server Envoy proxy.
var expectedPrometheusServerStats = map[string]env.Stat{
	"istio_requests_total": {Value: 10},
	"istio_build":          {Value: 1},
}

func TestStatsPlugin(t *testing.T) {
	testStatsPlugin(t, true, func(s *env.TestSetup) {
		s.VerifyPrometheusStats(expectedPrometheusServerStats, s.Ports().ServerAdminPort)
		clntStats := map[string]env.Stat{
			"istio_requests_total": {Value: 10, Labels: map[string]string{"destination_service": "unknown"}},
			"istio_build":          {Value: 1},
		}
		s.VerifyPrometheusStats(clntStats, s.Ports().ClientAdminPort)
	})
}

func TestStatsPluginHHFallback(t *testing.T) {
	testStatsPlugin(t, false, func(s *env.TestSetup) {
		s.VerifyPrometheusStats(expectedPrometheusServerStats, s.Ports().ServerAdminPort)
		clntStats := map[string]env.Stat{
			"istio_requests_total": {Value: 10, Labels: map[string]string{"destination_service": fmt.Sprintf("127.0.0.1:%d", s.Ports().AppToClientProxyPort)}},
			"istio_build":          {Value: 1},
		}
		s.VerifyPrometheusStats(clntStats, s.Ports().ClientAdminPort)
	})
}

type verifyFn func(s *env.TestSetup)

func testStatsPlugin(t *testing.T, disable_host_header_fallback bool, fn verifyFn) {
	s := env.NewClientServerEnvoyTestSetup(env.StatsPluginTest, t)
	s.SetFiltersBeforeEnvoyRouterInClientToProxy(fmt.Sprintf(outboundStatsFilter, disable_host_header_fallback))
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
