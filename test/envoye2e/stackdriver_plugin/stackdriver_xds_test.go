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
	"strconv"
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const StackdriverClientHTTPListener = `
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
              configuration: |
                { "max_peer_cache_size": 20 }
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

const StackdriverServerHTTPListener = `
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
              configuration: |
                { "max_peer_cache_size": 20 }
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
                {"enableMeshEdgesReporting": "true", "meshEdgesReportingDuration": "1s"}
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

const StackdriverAndAccessLogFilter = `
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
              configuration: |
                { "max_peer_cache_size": 20 }
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              vm_config:
                runtime: "envoy.wasm.runtime.null"
                code:
                  local: { inline_string: "envoy.wasm.access_log_policy" }
              configuration: "{ log_window_duration: \"{{ .Vars.LogWindowDuration }}\" }"
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
{{ .Vars.ServerTLSContext | indent 2 }}`

func TestStackdriverPayload(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]driver.SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  "testdata/stackdriver/server_access_log_entry.yaml.tmpl",
						LogEntryCount: 10,
					},
				},
				[]string{"testdata/stackdriver/traffic_assertion.yaml.tmpl"},
			),
			&driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
				"envoy_type_logging_success_true_export_call": &driver.ExactStat{"testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadGateway(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"RequestPath":           "echo",
		"SDLogStatusCode":       "200",
		"StackdriverRootCAFile": driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":  driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":           driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "server", Version: "0",
				Clusters:  []string{driver.LoadTestData("testdata/cluster/server.yaml.tmpl")},
				Listeners: []string{StackdriverClientHTTPListener, StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 1, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				nil,
				[]driver.SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/gateway_access_log.yaml.tmpl",
						LogEntryFile:  "testdata/stackdriver/gateway_access_log_entry.yaml.tmpl",
						LogEntryCount: 1,
					},
				},
				nil,
			),
			&driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
				"envoy_type_logging_success_true_export_call": &driver.ExactStat{"testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadWithTLS(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "MUTUAL_TLS",
		"SDLogStatusCode":             "200",
		"SourcePrincipal":             "spiffe://cluster.local/ns/default/sa/client",
		"DestinationPrincipal":        "spiffe://cluster.local/ns/default/sa/server",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ClientTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
	params.Vars["ServerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]driver.SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  "testdata/stackdriver/server_access_log_entry.yaml.tmpl",
						LogEntryCount: 10,
					},
				},
				nil,
			),
			&driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
				"envoy_type_logging_success_true_export_call": &driver.ExactStat{"testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

// Expects estimated 20s log dumping interval from stackdriver
func TestStackdriverReload(t *testing.T) {
	env.SkipTSanASan(t)
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{2 * time.Second},
			&driver.Repeat{N: 5, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Sleep{2 * time.Second},
			&driver.Repeat{N: 5, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]driver.SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  "testdata/stackdriver/server_access_log_entry.yaml.tmpl",
						LogEntryCount: 10,
					},
				},
				nil,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverVMReload(t *testing.T) {
	env.SkipTSanASan(t)
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"ReloadVM":                    "true",
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Sleep{1 * time.Second},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{StackdriverServerHTTPListener}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]driver.SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  "testdata/stackdriver/server_access_log_entry.yaml.tmpl",
						LogEntryCount: 10,
					},
				},
				nil,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverGCEInstances(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/gce_client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/gce_server_node_metadata.json.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Sleep{1 * time.Second},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{StackdriverServerHTTPListener}},
			sd.Check(params,
				[]string{"testdata/stackdriver/gce_client_request_count.yaml.tmpl", "testdata/stackdriver/gce_server_request_count.yaml.tmpl"},
				nil,
				[]string{"testdata/stackdriver/gce_traffic_assertion.yaml.tmpl"},
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

// Expects estimated 20s log dumping interval from stackdriver
func TestStackdriverParallel(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"SDLogStatusCode":       "200",
		"StackdriverRootCAFile": driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":  driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	sd := &driver.Stackdriver{Port: params.Ports.SDPort, Delay: 100 * time.Millisecond}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			driver.Get(params.Ports.ClientPort, "hello, world!"),
			&driver.Fork{
				Fore: &driver.Scenario{
					[]driver.Step{
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{
							Duration: 19 * time.Second,
							Step:     driver.Get(params.Ports.ClientPort, "hello, world!"),
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

func TestStackdriverAccessLog(t *testing.T) {
	var TestCases = []struct {
		name              string
		logWindowDuration string
		sleepDuration     time.Duration
		respCode          string
		logEntryCount     int
	}{
		{"StackdriverAndAccessLogPlugin", "15s", 0, "200", 1},
		{"RequestGetsLoggedAgain", "1s", 1 * time.Second, "200", 2},
		{"AllErrorRequestsGetsLogged", "1s", 0, "500", 10},
	}

	for _, tt := range TestCases {
		t.Run(tt.name, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"LogWindowDuration":           tt.logWindowDuration,
				"EnableMetadataExchange":      "true",
				"ServiceAuthenticationPolicy": "NONE",
				"DirectResponseCode":          tt.respCode,
				"SDLogStatusCode":             tt.respCode,
				"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
				"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
			}, envoye2e.ProxyE2ETests)

			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

			sd := &driver.Stackdriver{Port: params.Ports.SDPort}
			respCode, _ := strconv.Atoi(tt.respCode)
			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					sd,
					&driver.SecureTokenService{Port: params.Ports.STSPort},
					&driver.Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
					&driver.Update{Node: "server", Version: "0", Listeners: []string{StackdriverAndAccessLogFilter}},
					&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
					&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
					&driver.Sleep{Duration: 1 * time.Second},
					&driver.Repeat{
						N: 5,
						Step: &driver.HTTPCall{
							Port:         params.Ports.ClientPort,
							ResponseCode: respCode,
						},
					},
					&driver.Sleep{Duration: tt.sleepDuration},
					&driver.Repeat{
						N: 5,
						Step: &driver.HTTPCall{
							Port:         params.Ports.ClientPort,
							ResponseCode: respCode,
						},
					},
					sd.Check(params,
						nil,
						[]driver.SDLogEntry{
							{
								LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
								LogEntryFile:  "testdata/stackdriver/server_access_log_entry.yaml.tmpl",
								LogEntryCount: tt.logEntryCount,
							},
						},
						nil,
					),
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}
