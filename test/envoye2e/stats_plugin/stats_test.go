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

package client

import (
	"fmt"
	"os"
	"strconv"
	"testing"
	"time"

	dto "github.com/prometheus/client_model/go"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

func skipWasm(t *testing.T, runtime string) {
	if os.Getenv("WASM") != "" {
		if runtime != "envoy.wasm.runtime.v8" {
			t.Skip("Skip test since runtime is not v8")
		}
	} else if runtime == "envoy.wasm.runtime.v8" {
		t.Skip("Skip v8 runtime test since wasm module is not generated")
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

var Runtimes = []struct {
	MetadataExchangeFilterCode string
	StatsFilterCode            string
	WasmRuntime                string
}{
	{
		MetadataExchangeFilterCode: "inline_string: \"envoy.wasm.metadata_exchange\"",
		StatsFilterCode:            "inline_string: \"envoy.wasm.stats\"",
		WasmRuntime:                "envoy.wasm.runtime.null",
	},
	{
		MetadataExchangeFilterCode: "filename: extensions/metadata_exchange/plugin.wasm",
		StatsFilterCode:            "filename: extensions/stats/plugin.wasm",
		WasmRuntime:                "envoy.wasm.runtime.v8",
	},
}

var TestCases = []struct {
	Name              string
	ClientConfig      string
	ServerClusterName string
	ClientStats       map[string]driver.StatMatcher
	ServerStats       map[string]driver.StatMatcher
}{
	{
		Name:         "Default",
		ClientConfig: "testdata/stats/client_config.yaml",
		ClientStats: map[string]driver.StatMatcher{
			"istio_requests_total": &driver.ExactStat{"testdata/metric/client_request_total.yaml.tmpl"},
		},
		ServerStats: map[string]driver.StatMatcher{
			"istio_requests_total": &driver.ExactStat{"testdata/metric/server_request_total.yaml.tmpl"},
			"istio_build":          &driver.ExactStat{"testdata/metric/istio_build.yaml"},
		},
	},
	{
		Name:         "Customized",
		ClientConfig: "testdata/stats/client_config_customized.yaml",
		ClientStats: map[string]driver.StatMatcher{
			"istio_custom":         &driver.ExactStat{"testdata/metric/client_custom_metric.yaml.tmpl"},
			"istio_requests_total": &driver.ExactStat{"testdata/metric/client_request_total_customized.yaml.tmpl"},
		},
		ServerStats: map[string]driver.StatMatcher{
			"istio_requests_total": &driver.ExactStat{"testdata/metric/server_request_total.yaml.tmpl"},
			"istio_build":          &driver.ExactStat{"testdata/metric/istio_build.yaml"},
		},
	},
	{
		Name:              "UseHostHeader",
		ClientConfig:      "testdata/stats/client_config.yaml",
		ServerClusterName: "host_header",
		ClientStats: map[string]driver.StatMatcher{
			"istio_requests_total": &driver.ExactStat{"testdata/metric/host_header_fallback.yaml.tmpl"},
		},
		ServerStats: map[string]driver.StatMatcher{},
	},
	{
		Name:              "DisableHostHeader",
		ClientConfig:      "testdata/stats/client_config_disable_header_fallback.yaml",
		ServerClusterName: "host_header",
		ClientStats: map[string]driver.StatMatcher{
			"istio_requests_total": &driver.ExactStat{"testdata/metric/disable_host_header_fallback.yaml.tmpl"},
		},
		ServerStats: map[string]driver.StatMatcher{},
	},
}

func TestStatsPayload(t *testing.T) {
	env.SkipTSanASan(t)
	for _, testCase := range TestCases {
		for _, runtime := range Runtimes {
			t.Run(testCase.Name+"/"+runtime.WasmRuntime, func(t *testing.T) {
				skipWasm(t, runtime.WasmRuntime)
				params := driver.NewTestParams(t, map[string]string{
					"RequestCount":               "10",
					"EnableMetadataExchange":     "true",
					"MetadataExchangeFilterCode": runtime.MetadataExchangeFilterCode,
					"StatsFilterCode":            runtime.StatsFilterCode,
					"WasmRuntime":                runtime.WasmRuntime,
					"StatsConfig":                driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
					"StatsFilterClientConfig":    driver.LoadTestJSON(testCase.ClientConfig),
					"StatsFilterServerConfig":    driver.LoadTestJSON("testdata/stats/server_config.yaml"),
					"ServerClusterName":          testCase.ServerClusterName,
				}, envoye2e.ProxyE2ETests)
				params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
				params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
				params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stats_inbound.yaml.tmpl")
				params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stats_outbound.yaml.tmpl")
				if err := (&driver.Scenario{
					[]driver.Step{
						&driver.XDS{},
						&driver.Update{
							Node:      "client",
							Version:   "0",
							Clusters:  []string{params.LoadTestData("testdata/cluster/server.yaml.tmpl")},
							Listeners: []string{params.LoadTestData("testdata/listener/client.yaml.tmpl")}},
						&driver.Update{Node: "server", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/server.yaml.tmpl")}},
						&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
						&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{N: 10,
							Step: &driver.HTTPCall{
								Port: params.Ports.ClientPort,
								Body: "hello, world!",
							},
						},
						&driver.Stats{params.Ports.ClientAdmin, testCase.ClientStats},
						&driver.Stats{params.Ports.ServerAdmin, testCase.ServerStats},
					},
				}).Run(params); err != nil {
					t.Fatal(err)
				}
			})
		}
	}
}

func TestStatsParallel(t *testing.T) {
	env.SkipTSanASan(t)
	params := driver.NewTestParams(t, map[string]string{
		"RequestCount":               "1",
		"MetadataExchangeFilterCode": "inline_string: \"envoy.wasm.metadata_exchange\"",
		"StatsFilterCode":            "inline_string: \"envoy.wasm.stats\"",
		"WasmRuntime":                "envoy.wasm.runtime.null",
		"EnableMetadataExchange":     "true",
		"StatsConfig":                driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
		"StatsFilterClientConfig":    driver.LoadTestJSON("testdata/stats/client_config.yaml"),
		"StatsFilterServerConfig":    driver.LoadTestJSON("testdata/stats/server_config.yaml"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	clientRequestTotal := &dto.MetricFamily{}
	serverRequestTotal := &dto.MetricFamily{}
	params.LoadTestProto("testdata/metric/client_request_total.yaml.tmpl", clientRequestTotal)
	params.LoadTestProto("testdata/metric/server_request_total.yaml.tmpl", serverRequestTotal)
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stats_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stats_outbound.yaml.tmpl")

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			driver.Get(params.Ports.ClientPort, "hello, world!"),
			&driver.Fork{
				Fore: &driver.Scenario{
					[]driver.Step{
						&driver.Sleep{1 * time.Second},
						&driver.Repeat{
							Duration: 9 * time.Second,
							Step:     driver.Get(params.Ports.ClientPort, "hello, world!"),
						},
						capture{},
					},
				},
				Back: &driver.Repeat{
					Duration: 10 * time.Second,
					Step: &driver.Scenario{
						[]driver.Step{
							&driver.Update{
								Node:      "client",
								Version:   "{{.N}}",
								Listeners: []string{params.LoadTestData("testdata/listener/client.yaml.tmpl")},
							},
							&driver.Update{
								Node:      "server",
								Version:   "{{.N}}",
								Listeners: []string{params.LoadTestData("testdata/listener/server.yaml.tmpl")},
							},
							// may need short delay so we don't eat all the CPU
							&driver.Sleep{100 * time.Millisecond},
						},
					},
				},
			},
			&driver.Stats{params.Ports.ClientAdmin, map[string]driver.StatMatcher{
				"istio_requests_total": &driver.ExactStat{"testdata/metric/client_request_total.yaml.tmpl"},
			}},
			&driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
				"istio_requests_total": &driver.ExactStat{"testdata/metric/server_request_total.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStatsGrpc(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"RequestCount":               "10",
		"MetadataExchangeFilterCode": "inline_string: \"envoy.wasm.metadata_exchange\"",
		"StatsFilterCode":            "inline_string: \"envoy.wasm.stats\"",
		"WasmRuntime":                "envoy.wasm.runtime.null",
		"DisableDirectResponse":      "true",
		"UsingGrpcBackend":           "true",
		"GrpcResponseStatus":         "7",
		"EnableMetadataExchange":     "true",
		"StatsConfig":                driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
		"StatsFilterClientConfig":    driver.LoadTestJSON("testdata/stats/client_config.yaml"),
		"StatsFilterServerConfig":    driver.LoadTestJSON("testdata/stats/server_config.yaml"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stats_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stats_outbound.yaml.tmpl")

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.GrpcServer{},
			&driver.GrpcCall{
				ReqCount:   10,
				WantStatus: status.New(codes.PermissionDenied, "denied"),
			},
			&driver.Stats{
				AdminPort: params.Ports.ServerAdmin,
				Matchers: map[string]driver.StatMatcher{
					"istio_requests_total": &driver.ExactStat{Metric: "testdata/metric/server_request_total.yaml.tmpl"},
				}},
			&driver.Stats{
				AdminPort: params.Ports.ClientAdmin,
				Matchers: map[string]driver.StatMatcher{
					"istio_requests_total": &driver.ExactStat{Metric: "testdata/metric/client_request_total.yaml.tmpl"},
				}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
