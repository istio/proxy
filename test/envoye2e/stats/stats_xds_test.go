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

package stats

import (
	"fmt"
	"strconv"
	"testing"
	"time"

	dto "github.com/prometheus/client_model/go"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const StatsClientHTTPListener = `
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
                runtime: {{ .Vars.WasmRuntime }}
                code:
                  local: { {{ .Vars.MetadataExchangeFilterCode }} }
              configuration: "test"
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              root_id: "stats_outbound"
              vm_config:
                vm_id: stats_outbound{{ .N }}
                runtime: {{ .Vars.WasmRuntime }}
                code:
                  local: { {{ .Vars.StatsFilterCode }} }
              configuration: |
                {{ .Vars.StatsFilterClientConfig }}
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

const StatsServerHTTPListener = `
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
                runtime: {{ .Vars.WasmRuntime }}
                code:
                  local: { {{ .Vars.MetadataExchangeFilterCode }} }
              configuration: "test"
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              root_id: "stats_inbound"
              vm_config:
                vm_id: stats_inbound{{ .N }}
                runtime: {{ .Vars.WasmRuntime }}
                code:
                  local: { {{ .Vars.StatsFilterCode }} }
              configuration: |
                {{ .Vars.StatsFilterServerConfig }}
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

func skipTSAN(t *testing.T) {
	if os.Getenv("TSAN") != "" {
		t.Skip("https://github.com/istio/istio/issues/21273")
	}
}

type capture struct{}

func (capture) Run(p *driver.Params) error {
	prev, err := strconv.Atoi(p.Vars["RequestCount"])
	if err != nil {
		return err
	}
	p.Vars["RequestCount"] = fmt.Sprintf("%d", p.N+prev)
	return nil
}
func (capture) Cleanup() {}

var TestCases = []struct {
	Ports                      uint16
	MetadataExchangeFilterCode string
	StatsFilterCode            string
	WasmRuntime                string
}{
	{
		Ports:                      env.StatsPayload,
		MetadataExchangeFilterCode: "inline_string: \"envoy.wasm.metadata_exchange\"",
		StatsFilterCode:            "inline_string: \"envoy.wasm.stats\"",
		WasmRuntime:                "envoy.wasm.runtime.null",
	},
	{
		Ports:                      env.StatsWasm,
		MetadataExchangeFilterCode: "filename: extensions/metadata_exchange/plugin.wasm",
		StatsFilterCode:            "filename: extensions/stats/plugin.wasm",
		WasmRuntime:                "envoy.wasm.runtime.v8",
	},
}

var ClientConfigs = []struct {
	Name         string
	ClientConfig string
	ClientStats  map[string]driver.StatMatcher
}{
	{
		Name:         "Default",
		ClientConfig: "testdata/stats/client_config.yaml",
		ClientStats: map[string]driver.StatMatcher{
			"istio_requests_total": &driver.ExactStat{"testdata/metric/client_request_total.yaml.tmpl"},
		},
	},
	{
		Name:         "Customized",
		ClientConfig: "testdata/stats/client_config_customized.yaml",
		ClientStats: map[string]driver.StatMatcher{
			"istio_custom":         &driver.ExactStat{"testdata/metric/client_custom_metric.yaml.tmpl"},
			"istio_requests_total": &driver.ExactStat{"testdata/metric/client_request_total_customized.yaml.tmpl"},
		},
	},
}

func TestStatsPayload(t *testing.T) {
	skipTSAN(t)
	for _, config := range ClientConfigs {
		for _, testCase := range TestCases {
			t.Run(config.Name+"/"+testCase.WasmRuntime, func(t *testing.T) {
				ports := env.NewPorts(testCase.Ports)
				params := &driver.Params{
					Vars: map[string]string{
						"ClientPort":                 fmt.Sprintf("%d", ports.AppToClientProxyPort),
						"BackendPort":                fmt.Sprintf("%d", ports.BackendPort),
						"ClientAdmin":                fmt.Sprintf("%d", ports.ClientAdminPort),
						"ServerAdmin":                fmt.Sprintf("%d", ports.ServerAdminPort),
						"ServerPort":                 fmt.Sprintf("%d", ports.ClientToServerProxyPort),
						"RequestCount":               "10",
						"MetadataExchangeFilterCode": testCase.MetadataExchangeFilterCode,
						"StatsFilterCode":            testCase.StatsFilterCode,
						"WasmRuntime":                testCase.WasmRuntime,
						"StatsConfig":                driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
						"StatsFilterClientConfig":    driver.LoadTestJSON(config.ClientConfig),
						"StatsFilterServerConfig":    driver.LoadTestJSON("testdata/stats/server_config.yaml"),
					},
					XDS: int(ports.XDSPort),
				}
				params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
				params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")

				if err := (&driver.Scenario{
					[]driver.Step{
						&driver.XDS{},
						&driver.Update{Node: "client", Version: "0", Listeners: []string{StatsClientHTTPListener}},
						&driver.Update{Node: "server", Version: "0", Listeners: []string{StatsServerHTTPListener}},
						&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
						&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{N: 10, Step: &driver.Get{ports.AppToClientProxyPort, "hello, world!"}},
						&driver.Stats{ports.ClientAdminPort, config.ClientStats},
						&driver.Stats{ports.ServerAdminPort, map[string]driver.StatMatcher{
							"istio_requests_total": &driver.ExactStat{"testdata/metric/server_request_total.yaml.tmpl"},
						}},
					},
				}).Run(params); err != nil {
					t.Fatal(err)
				}
			})
		}
	}
}

func TestStatsParallel(t *testing.T) {
	skipTSAN(t)
	ports := env.NewPorts(env.StatsParallel)
	params := &driver.Params{
		Vars: map[string]string{
			"ClientPort":                 fmt.Sprintf("%d", ports.AppToClientProxyPort),
			"BackendPort":                fmt.Sprintf("%d", ports.BackendPort),
			"ClientAdmin":                fmt.Sprintf("%d", ports.ClientAdminPort),
			"ServerAdmin":                fmt.Sprintf("%d", ports.ServerAdminPort),
			"ServerPort":                 fmt.Sprintf("%d", ports.ClientToServerProxyPort),
			"RequestCount":               "1",
			"MetadataExchangeFilterCode": "inline_string: \"envoy.wasm.metadata_exchange\"",
			"StatsFilterCode":            "inline_string: \"envoy.wasm.stats\"",
			"WasmRuntime":                "envoy.wasm.runtime.null",
			"StatsConfig":                driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
			"StatsFilterClientConfig":    driver.LoadTestJSON("testdata/stats/client_config.yaml"),
			"StatsFilterServerConfig":    driver.LoadTestJSON("testdata/stats/server_config.yaml"),
		},
		XDS: int(ports.XDSPort),
	}
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	clientRequestTotal := &dto.MetricFamily{}
	serverRequestTotal := &dto.MetricFamily{}
	params.LoadTestProto("testdata/metric/client_request_total.yaml.tmpl", clientRequestTotal)
	params.LoadTestProto("testdata/metric/server_request_total.yaml.tmpl", serverRequestTotal)

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{StatsClientHTTPListener}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{StatsServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Get{ports.AppToClientProxyPort, "hello, world!"},
			&driver.Fork{
				Fore: &driver.Scenario{
					[]driver.Step{
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{
							Duration: 9 * time.Second,
							Step:     &driver.Get{ports.AppToClientProxyPort, "hello, world!"},
						},
						capture{},
					},
				},
				Back: &driver.Repeat{
					Duration: 10 * time.Second,
					Step: &driver.Scenario{
						[]driver.Step{
							&driver.Update{Node: "client", Version: "{{.N}}", Listeners: []string{StatsClientHTTPListener}},
							&driver.Update{Node: "server", Version: "{{.N}}", Listeners: []string{StatsServerHTTPListener}},
							// may need short delay so we don't eat all the CPU
							&driver.Sleep{100 * time.Millisecond},
						},
					},
				},
			},
			&driver.Stats{ports.ClientAdminPort, map[string]driver.StatMatcher{
				"istio_requests_total": &driver.ExactStat{"testdata/metric/client_request_total.yaml.tmpl"},
			}},
			&driver.Stats{ports.ServerAdminPort, map[string]driver.StatMatcher{
				"istio_requests_total": &driver.ExactStat{"testdata/metric/server_request_total.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
