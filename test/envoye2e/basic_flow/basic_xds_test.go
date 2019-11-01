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
	"time"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const ClientHTTPListener = `
name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Vars.ClientPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    config:
      codec_type: AUTO
      stat_prefix: client
      http_filters:
      - name: envoy.router
      route_config:
        name: client
        virtual_hosts:
        - name: client
          domains: ["*"]
          routes:
          - match: { prefix: / }
            route:
              cluster: server
              timeout: 0s
`

const ServerHTTPListener = `
name: server
traffic_direction: INBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Vars.ServerPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    config:
      codec_type: AUTO
      stat_prefix: server
      http_filters:
      - name: envoy.router
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

func TestBasicHTTP(t *testing.T) {
	ports := env.NewPorts(env.BasicHTTP)
	params := &driver.Params{
		Vars: map[string]string{
			"BackendPort": fmt.Sprintf("%d", ports.BackendPort),
			"ClientPort":  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"ClientAdmin": fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin": fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":  fmt.Sprintf("%d", ports.ProxyToServerProxyPort),
		},
		XDS: int(ports.XDSPort),
	}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{ClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{ServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Get{ports.ClientToServerProxyPort, "hello, world!"},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestBasicHTTPwithTLS(t *testing.T) {
	ports := env.NewPorts(env.BasicHTTPwithTLS)
	params := &driver.Params{
		Vars: map[string]string{
			"BackendPort": fmt.Sprintf("%d", ports.BackendPort),
			"ClientPort":  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"ClientAdmin": fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin": fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":  fmt.Sprintf("%d", ports.ProxyToServerProxyPort),
		},
		XDS: int(ports.XDSPort),
	}
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
			&driver.Get{ports.ClientToServerProxyPort, "hello, world!"},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
