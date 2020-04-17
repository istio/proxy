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
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
)

const ServerMXFilter = `
- name: envoy.filters.network.metadata_exchange
  typed_config: 
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: envoy.tcp.metadataexchange.config.MetadataExchange
    value:
      protocol: mx-protocol`

const ClientMXFilter = `
- name: envoy.filters.network.upstream.metadata_exchange
  typed_config: 
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: envoy.tcp.metadataexchange.config.MetadataExchange
    value:
      protocol: mx-protocol`

const ServerStatsFilter = `
- name: envoy.filters.network.wasm
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: envoy.extensions.filters.http.wasm.v3.Wasm
    value:
      config:
        root_id: "stats_inbound"
        vm_config:
          runtime: envoy.wasm.runtime.null
          code:
            local: { inline_string: "envoy.wasm.stats" }
        configuration: |
          { "debug": "false", "field_separator": ";.;", "tcp_reporting_duration": "1s" }`

const ClientStatsFilter = `
- name: envoy.filters.network.wasm
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: envoy.extensions.filters.http.wasm.v3.Wasm
    value:
      config:
        root_id: "stats_outbound"
        vm_config:
          runtime: envoy.wasm.runtime.null
          code:
            local: { inline_string: "envoy.wasm.stats" }
        configuration: |
          { "debug": "false", "field_separator": ";.;", "tcp_reporting_duration": "1s" }`

const ClientTransportSocket = `transport_socket:
  name: tls
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext
    value:
      common_tls_context:
        alpn_protocols:
        - {{ .Vars.AlpnProtocol }}
        tls_certificates:
        - certificate_chain: { filename: "testdata/certs/client.cert" }
          private_key: { filename: "testdata/certs/client-key.cert" }
        validation_context:
          trusted_ca: { filename: "testdata/certs/root.cert" }`

const ServerTransportSocket = `transport_socket:
  name: tls
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: envoy.extensions.transport_sockets.tls.v3.DownstreamTlsContext
    value:
      common_tls_context:
        alpn_protocols:
        - {{ .Vars.AlpnProtocol }}
        tls_certificates:
        - certificate_chain: { filename: "testdata/certs/server.cert" }
          private_key: { filename: "testdata/certs/server-key.cert" }
        validation_context:
          trusted_ca: { filename: "testdata/certs/root.cert" }
      require_client_certificate: true`

func TestTCPMetadataExchange(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ServerNetworkFilters":     ServerMXFilter + ServerStatsFilter,
		"ClientNetworkFilters":     ClientStatsFilter,
		"ClientUpstreamFilters":    ClientMXFilter,
		"DisableDirectResponse":    "true",
		"AlpnProtocol":             "mx-protocol",
		"ClientClusterTLSContext":  ClientTransportSocket,
		"ServerListenerTLSContext": ServerTransportSocket,
		"StatsConfig":              driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Update{
				Node:      "client",
				Version:   "0",
				Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_client.yaml.tmpl")},
				Listeners: []string{params.LoadTestData("testdata/listener/tcp_client.yaml.tmpl")},
			},
			&driver.Update{
				Node:      "server",
				Version:   "0",
				Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_server.yaml.tmpl")},
				Listeners: []string{params.LoadTestData("testdata/listener/tcp_server.yaml.tmpl")},
			},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.TCPServer{Prefix: "hello"},
			&driver.Repeat{
				N:    10,
				Step: &driver.TCPConnection{},
			},
			&driver.Stats{params.Ports.ClientAdmin, map[string]driver.StatMatcher{
				"istio_tcp_connections_closed_total": &driver.ExactStat{"testdata/metric/tcp_client_connection_close.yaml.tmpl"},
				"istio_tcp_connections_opened_total": &driver.ExactStat{"testdata/metric/tcp_client_connection_open.yaml.tmpl"},
				"istio_tcp_received_bytes_total":     &driver.ExactStat{"testdata/metric/tcp_client_received_bytes.yaml.tmpl"},
				"istio_tcp_sent_bytes_total":         &driver.ExactStat{"testdata/metric/tcp_client_sent_bytes.yaml.tmpl"},
			}},
			&driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
				"istio_tcp_connections_closed_total":          &driver.ExactStat{"testdata/metric/tcp_server_connection_close.yaml.tmpl"},
				"istio_tcp_connections_opened_total":          &driver.ExactStat{"testdata/metric/tcp_server_connection_open.yaml.tmpl"},
				"istio_tcp_received_bytes_total":              &driver.ExactStat{"testdata/metric/tcp_server_received_bytes.yaml.tmpl"},
				"istio_tcp_sent_bytes_total":                  &driver.ExactStat{"testdata/metric/tcp_server_sent_bytes.yaml.tmpl"},
				"envoy_metadata_exchange_alpn_protocol_found": &driver.ExactStat{"testdata/metric/tcp_server_mx_stats_alpn_found.yaml.tmpl"},
				"envoy_metadata_exchange_metadata_added":      &driver.ExactStat{"testdata/metric/tcp_server_mx_stats_metadata_added.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestTCPMetadataExchangeNoAlpn(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ServerNetworkFilters":     ServerMXFilter + ServerStatsFilter,
		"DisableDirectResponse":    "true",
		"AlpnProtocol":             "some-protocol",
		"ClientClusterTLSContext":  ClientTransportSocket,
		"ServerListenerTLSContext": ServerTransportSocket,
		"StatsConfig":              driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Update{
				Node:      "client",
				Version:   "0",
				Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_client.yaml.tmpl")},
				Listeners: []string{params.LoadTestData("testdata/listener/tcp_client.yaml.tmpl")},
			},
			&driver.Update{
				Node:      "server",
				Version:   "0",
				Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_server.yaml.tmpl")},
				Listeners: []string{params.LoadTestData("testdata/listener/tcp_server.yaml.tmpl")},
			},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.TCPServer{Prefix: "hello"},
			&driver.Repeat{
				N:    10,
				Step: &driver.TCPConnection{},
			},
			&driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
				"istio_tcp_connections_opened_total":              &driver.ExactStat{"testdata/metric/tcp_server_connection_open_without_mx.yaml.tmpl"},
				"envoy_metadata_exchange_alpn_protocol_not_found": &driver.ExactStat{"testdata/metric/tcp_server_mx_stats_alpn_not_found.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
