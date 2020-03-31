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
name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Vars.ClientPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: client{{ .N }}
      http_filters:
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              vm_config:
                runtime: "envoy.wasm.runtime.null"
                code:
                  local: { inline_string: "envoy.wasm.metadata_exchange" }
              configuration: "test"
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              root_id: "stackdriver_outbound"
              vm_config:
                {{- if .Vars.ReloadVM }}
                vm_id: "stackdriver_outbound_{{ .Vars.Version }}"
                {{- else }}
                vm_id: "stackdriver_outbound"
                {{- end }}
                runtime: "envoy.wasm.runtime.null"
                code:
                  local: { inline_string: "envoy.wasm.null.stackdriver" }
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
name: server
traffic_direction: INBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Vars.ServerPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: server{{ .N }}
      http_filters:
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              vm_config:
                runtime: "envoy.wasm.runtime.null"
                code:
                  local: { inline_string: "envoy.wasm.metadata_exchange" }
              configuration: "test"
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              root_id: "stackdriver_inbound"
              vm_config:
                {{- if .Vars.ReloadVM }}
                vm_id: "stackdriver_inbound_{{ .Vars.Version }}"
                {{- else }}
                vm_id: "stackdriver_inbound"
                {{- end }}
                runtime: "envoy.wasm.runtime.null"
                code:
                  local: { inline_string: "envoy.wasm.null.stackdriver" }
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
			"ClientPort":                  fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"SDPort":                      fmt.Sprintf("%d", ports.SDPort),
			"STSPort":                     fmt.Sprintf("%d", ports.STSPort),
			"BackendPort":                 fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                 fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                 fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"ServiceAuthenticationPolicy": "NONE",
			"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
			"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
			"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
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
			&driver.SecureTokenService{Port: ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]string{"testdata/stackdriver/server_access_log.yaml.tmpl"},
			),
			&driver.Stats{ports.ServerAdminPort, map[string]driver.StatMatcher{
				"envoy_type_logging_success_true_export_call": &driver.ExactStat{"testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadGateway(t *testing.T) {
	ports := env.NewPorts(env.StackDriverPayloadGateway)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":            fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"SDPort":                fmt.Sprintf("%d", ports.SDPort),
			"STSPort":               fmt.Sprintf("%d", ports.STSPort),
			"BackendPort":           fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":           fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":           fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":            fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"RequestPath":           "echo",
			"StackdriverRootCAFile": driver.TestPath("testdata/certs/stackdriver.pem"),
			"StackdriverTokenFile":  driver.TestPath("testdata/certs/access-token"),
			"StatsConfig":           driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
		},
		XDS: int(ports.XDSPort),
	}
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: ports.STSPort},
			&driver.Update{Node: "server", Version: "0",
				Clusters:  []string{driver.LoadTestData("testdata/cluster/server.yaml.tmpl")},
				Listeners: []string{StackdriverClientHTTPListener, StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 1, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
			sd.Check(params,
				nil,
				[]string{"testdata/stackdriver/gateway_access_log.yaml.tmpl"},
			),
			&driver.Stats{ports.ServerAdminPort, map[string]driver.StatMatcher{
				"envoy_type_logging_success_true_export_call": &driver.ExactStat{"testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadWithTLS(t *testing.T) {
	ports := env.NewPorts(env.StackDriverPayloadWithTLS)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":                  fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"SDPort":                      fmt.Sprintf("%d", ports.SDPort),
			"STSPort":                     fmt.Sprintf("%d", ports.STSPort),
			"BackendPort":                 fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                 fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                 fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"ServiceAuthenticationPolicy": "MUTUAL_TLS",
			"SourcePrincipal":             "spiffe://cluster.local/ns/default/sa/client",
			"DestinationPrincipal":        "spiffe://cluster.local/ns/default/sa/server",
			"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
			"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
			"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
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
			&driver.SecureTokenService{Port: ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]string{"testdata/stackdriver/server_access_log.yaml.tmpl"},
			),
			&driver.Stats{ports.ServerAdminPort, map[string]driver.StatMatcher{
				"envoy_type_logging_success_true_export_call": &driver.ExactStat{"testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

// Expects estimated 10s log dumping interval from stackdriver
func TestStackdriverReload(t *testing.T) {
	env.SkipTSanASan(t)
	ports := env.NewPorts(env.StackDriverReload)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":                  fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"SDPort":                      fmt.Sprintf("%d", ports.SDPort),
			"STSPort":                     fmt.Sprintf("%d", ports.STSPort),
			"BackendPort":                 fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                 fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                 fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"ServiceAuthenticationPolicy": "NONE",
			"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
			"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
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
			&driver.SecureTokenService{Port: ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{2 * time.Second},
			&driver.Repeat{N: 5, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Sleep{2 * time.Second},
			&driver.Repeat{N: 5, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]string{"testdata/stackdriver/server_access_log.yaml.tmpl"},
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverVMReload(t *testing.T) {
	env.SkipTSanASan(t)
	ports := env.NewPorts(env.StackDriverReload)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":                  fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"SDPort":                      fmt.Sprintf("%d", ports.SDPort),
			"STSPort":                     fmt.Sprintf("%d", ports.STSPort),
			"BackendPort":                 fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                 fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                 fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                  fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"ServiceAuthenticationPolicy": "NONE",
			"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
			"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
			"ReloadVM":                    "true",
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
			&driver.SecureTokenService{Port: ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
			&driver.Sleep{1 * time.Second},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{StackdriverServerHTTPListener}},
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
func TestStackdriverParallel(t *testing.T) {
	ports := env.NewPorts(env.StackDriverParallel)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":            fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"SDPort":                fmt.Sprintf("%d", ports.SDPort),
			"STSPort":               fmt.Sprintf("%d", ports.STSPort),
			"BackendPort":           fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":           fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":           fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":            fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"StackdriverRootCAFile": driver.TestPath("testdata/certs/stackdriver.pem"),
			"StackdriverTokenFile":  driver.TestPath("testdata/certs/access-token"),
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
			&driver.SecureTokenService{Port: ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Get{ports.AppToClientProxyPort, "hello, world!"},
			&driver.Fork{
				Fore: &driver.Scenario{
					[]driver.Step{
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{
							Duration: 19 * time.Second,
							Step:     &driver.Get{ports.AppToClientProxyPort, "hello, world!"},
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
							&driver.Sleep{100 * time.Millisecond},
						},
					},
				},
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
