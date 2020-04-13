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

const ClientHTTPListener = `
name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Ports.ClientPort }}
filter_chains:
- filters:
  - name: http
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: client
      http_filters:
      - name: envoy.filters.http.router
      route_config:
        name: client
        virtual_hosts:
        - name: client
          domains: ["*"]
          routes:
          - match: { prefix: / }
            route:
              cluster: outbound|9080|http|server.default.svc.cluster.local
              timeout: 0s
`

const ServerHTTPListener = `
name: server
traffic_direction: INBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Ports.ServerPort }}
filter_chains:
- filters:
  - name: http
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: server
      http_filters:
      - name: envoy.filters.http.router
      route_config:
        name: server
        virtual_hosts:
        - name: server
          domains: ["*"]
          routes:
          - match: { prefix: / }
            route:
              cluster: inbound|9080|http|server.default.svc.cluster.local
              timeout: 0s
{{ .Vars.ServerTLSContext | indent 2 }}
`

func TestBasicTCPFlow(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ConnectionCount":       "10",
		"DisableDirectResponse": "true",
	}, envoye2e.ProxyE2ETests)
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Update{
				Node:      "client",
				Version:   "0",
				Clusters:  []string{driver.LoadTestData("testdata/cluster/tcp_client.yaml.tmpl")},
				Listeners: []string{driver.LoadTestData("testdata/listener/tcp_client.yaml.tmpl")},
			},
			&driver.Update{
				Node:      "server",
				Version:   "0",
				Clusters:  []string{driver.LoadTestData("testdata/cluster/tcp_server.yaml.tmpl")},
				Listeners: []string{driver.LoadTestData("testdata/listener/tcp_server.yaml.tmpl")},
			},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.TCPServer{Prefix: "hello"},
			&driver.Repeat{
				N:    10,
				Step: &driver.TCPConnection{},
			},
			&driver.Stats{
				AdminPort: params.Ports.ServerAdmin,
				Matchers: map[string]driver.StatMatcher{
					"envoy_tcp_downstream_cx_total": &driver.PartialStat{Metric: "testdata/metric/basic_flow_server_tcp_connection.yaml.tmpl"},
				}},
			&driver.Stats{
				AdminPort: params.Ports.ClientAdmin,
				Matchers: map[string]driver.StatMatcher{
					"envoy_tcp_downstream_cx_total": &driver.PartialStat{Metric: "testdata/metric/basic_flow_client_tcp_connection.yaml.tmpl"},
				}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestBasicHTTP(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{ClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{ServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			driver.Get(params.Ports.ClientPort, "hello, world!"),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestBasicHTTPwithTLS(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	params.Vars["ClientTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
	params.Vars["ServerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{ClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{ServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			driver.Get(params.Ports.ClientPort, "hello, world!"),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

// Tests with a single combined proxy hosting inbound/outbound listeners
func TestBasicHTTPGateway(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "server", Version: "0",
				Clusters:  []string{driver.LoadTestData("testdata/cluster/server.yaml.tmpl")},
				Listeners: []string{ClientHTTPListener, ServerHTTPListener},
			},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			driver.Get(params.Ports.ClientPort, "hello, world!"),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
