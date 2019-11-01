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

const StackdriverClientHTTPListener = `
name: client{{ .N }}
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
      stat_prefix: client{{ .N }}
      http_filters:
      - name: envoy.filters.http.wasm
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
            root_id: "stackdriver_outbound"
            vm_config:
              vm_id: "stackdriver_outbound"
              runtime: "envoy.wasm.runtime.null"
              code:
                inline_string: "envoy.wasm.null.stackdriver"
            configuration: >-
              {}
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

const StackdriverServerHTTPListener = `
name: server{{ .N }}
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
      stat_prefix: server{{ .N }}
      http_filters:
      - name: envoy.filters.http.wasm
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
            root_id: "stackdriver_inbound"
            vm_config:
              vm_id: "stackdriver_inbound"
              runtime: "envoy.wasm.runtime.null"
              code:
                inline_string: "envoy.wasm.null.stackdriver"
            configuration: >-
              {}
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

func TestStackdriverPayload(t *testing.T) {
	ports := env.NewPorts(env.StackDriverPayload)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":                  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"SDPort":                      fmt.Sprintf("%d", ports.SDPort),
			"BackendPort":                 fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                 fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                 fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                  fmt.Sprintf("%d", ports.ProxyToServerProxyPort),
			"ServiceAuthenticationPolicy": "NONE",
		},
		XDS: int(ports.XDSPort),
	}
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: &driver.Get{ports.ClientToServerProxyPort, "hello, world!"}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]string{"testdata/stackdriver/server_access_log.yaml.tmpl"},
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadWithTLS(t *testing.T) {
	ports := env.NewPorts(env.StackDriverPayloadWithTLS)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":                  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"SDPort":                      fmt.Sprintf("%d", ports.SDPort),
			"BackendPort":                 fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                 fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                 fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                  fmt.Sprintf("%d", ports.ProxyToServerProxyPort),
			"ServiceAuthenticationPolicy": "MUTUAL_TLS",
			"SourcePrincipal":             "spiffe://cluster.local/ns/default/sa/client",
			"DestinationPrincipal":        "spiffe://cluster.local/ns/default/sa/server",
		},
		XDS: int(ports.XDSPort),
	}
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ClientTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
	params.Vars["ServerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")

	sd := &driver.Stackdriver{Port: ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: &driver.Get{ports.ClientToServerProxyPort, "hello, world!"}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]string{"testdata/stackdriver/server_access_log.yaml.tmpl"},
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

// Expects estimated 10s log dumping interval from stackdriver
func TestStackdriverReload(t *testing.T) {
	ports := env.NewPorts(env.StackDriverReload)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"SDPort":      fmt.Sprintf("%d", ports.SDPort),
			"BackendPort": fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin": fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin": fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":  fmt.Sprintf("%d", ports.ProxyToServerProxyPort),
		},
		XDS: int(ports.XDSPort),
	}
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: ports.SDPort}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Get{ports.ClientToServerProxyPort, "hello, world!"},
			// should drain all listeners
			&driver.Update{Node: "client", Version: "1"},
			&driver.Update{Node: "server", Version: "1"},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{
				N: 10,
				Step: &driver.Scenario{
					[]driver.Step{
						&driver.Update{Node: "client", Version: "i{{ .N }}", Listeners: []string{StackdriverClientHTTPListener}},
						&driver.Update{Node: "server", Version: "i{{ .N }}", Listeners: []string{StackdriverServerHTTPListener}},
						&driver.Sleep{1 * time.Second},
						&driver.Get{ports.ClientToServerProxyPort, "hello, world!"},
					},
				},
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

// Expects estimated 10s log dumping interval from stackdriver
func TestStackdriverParallel(t *testing.T) {
	ports := env.NewPorts(env.StackDriverParallel)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"SDPort":      fmt.Sprintf("%d", ports.SDPort),
			"BackendPort": fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin": fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin": fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":  fmt.Sprintf("%d", ports.ProxyToServerProxyPort),
		},
		XDS: int(ports.XDSPort),
	}
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	sd := &driver.Stackdriver{Port: ports.SDPort, Delay: 100 * time.Millisecond}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Get{ports.ClientToServerProxyPort, "hello, world!"},
			&driver.Fork{
				Fore: &driver.Scenario{
					[]driver.Step{
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{
							Duration: 19 * time.Second,
							Step:     &driver.Get{ports.ClientToServerProxyPort, "hello, world!"},
						},
					},
				},
				Back: &driver.Repeat{
					Duration: 20 * time.Second,
					Step: &driver.Scenario{
						[]driver.Step{
							&driver.Update{Node: "client", Version: "{{.N}}", Listeners: []string{StackdriverClientHTTPListener}},
							&driver.Update{Node: "server", Version: "{{.N}}", Listeners: []string{StackdriverServerHTTPListener}},
							// may need short delay so we don't eat all the CPU
							&driver.Sleep{10 * time.Millisecond},
						},
					},
				},
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
