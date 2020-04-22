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

func TestStackdriverPayload(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"EnableMetadataExchange":      "true",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
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
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"RequestPath":            "echo",
		"SDLogStatusCode":        "200",
		"EnableMetadataExchange": "true",
		"StackdriverRootCAFile":  driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":   driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":            driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "server", Version: "0",
				Clusters: []string{driver.LoadTestData("testdata/cluster/server.yaml.tmpl")},
				Listeners: []string{
					driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
					driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
				}},
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
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "MUTUAL_TLS",
		"SDLogStatusCode":             "200",
		"EnableMetadataExchange":      "true",
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
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
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
	t.Parallel()
	env.SkipTSanASan(t)
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"EnableMetadataExchange":      "true",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{2 * time.Second},
			&driver.Repeat{N: 5, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
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
	t.Parallel()
	env.SkipTSanASan(t)
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"EnableMetadataExchange":      "true",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"ReloadVM":                    "true",
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")

	sd := &driver.Stackdriver{Port: params.Ports.SDPort}
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Sleep{1 * time.Second},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
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

// Expects estimated 20s log dumping interval from stackdriver
func TestStackdriverParallel(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"SDLogStatusCode":        "200",
		"EnableMetadataExchange": "true",
		"StackdriverRootCAFile":  driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":   driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")
	sd := &driver.Stackdriver{Port: params.Ports.SDPort, Delay: 100 * time.Millisecond}

	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			sd,
			&driver.SecureTokenService{Port: params.Ports.STSPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
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
							&driver.Update{Node: "client", Version: "{{.N}}", Listeners: []string{
								driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
							}},
							&driver.Update{Node: "server", Version: "{{.N}}", Listeners: []string{
								driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
							}},
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
	t.Parallel()
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
			params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/access_log_policy.yaml.tmpl") + "\n" +
				params.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
			params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")

			sd := &driver.Stackdriver{Port: params.Ports.SDPort}
			respCode, _ := strconv.Atoi(tt.respCode)
			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					sd,
					&driver.SecureTokenService{Port: params.Ports.STSPort},
					&driver.Update{Node: "client", Version: "0", Listeners: []string{
						params.LoadTestData("testdata/listener/client.yaml.tmpl"),
					}},
					&driver.Update{Node: "server", Version: "0", Listeners: []string{
						params.LoadTestData("testdata/listener/server.yaml.tmpl"),
					}},
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
