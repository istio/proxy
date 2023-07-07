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

package stackdriverplugin

import (
	"strconv"
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

func enableStackDriver(t *testing.T, vars map[string]string) {
	t.Helper()
	vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")
}

func TestStackdriverPayload(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
			&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
				"type_logging_success_true_envoy_export_call": &driver.ExactStat{Metric: "testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadGateway(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"RequestPath":           "echo",
		"SDLogStatusCode":       "200",
		"StackdriverRootCAFile": driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":  driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":           driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{
				Node: "server", Version: "0",
				Clusters: []string{driver.LoadTestData("testdata/cluster/server.yaml.tmpl")},
				Listeners: []string{
					driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
					driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
				},
			},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 1, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				nil,
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/gateway_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/gateway_access_log_entry.yaml.tmpl"},
						LogEntryCount: 1,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_gateway_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_gateway_access_log_entry.yaml.tmpl"},
						LogEntryCount: 1,
					},
				}, true,
			),
			&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
				"type_logging_success_true_envoy_export_call": &driver.ExactStat{Metric: "testdata/metric/stackdriver_gateway_callout_metric.yaml.tmpl"},
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
		"SourcePrincipal":             "spiffe://cluster.local/ns/default/sa/client",
		"DestinationPrincipal":        "spiffe://cluster.local/ns/default/sa/server",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ClientTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
	params.Vars["ServerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
			&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
				"type_logging_success_true_envoy_export_call": &driver.ExactStat{Metric: "testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
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
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/access_log_policy.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/access_log_policy.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 2 * time.Second},
			&driver.Repeat{N: 5, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Sleep{Duration: 2 * time.Second},
			&driver.Repeat{N: 5, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverVMReload(t *testing.T) {
	t.Skip("See issue https://github.com/istio/istio/issues/26548")
	t.Parallel()
	env.SkipTSanASan(t)
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"ReloadVM":                    "true",
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Update{Node: "client", Version: "1", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "1", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverGCEInstances(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/gce_client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/gce_server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/gce_client_request_count.yaml.tmpl", "testdata/stackdriver/gce_server_request_count.yaml.tmpl"},
				nil, true,
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
		"SDLogStatusCode":       "200",
		"StackdriverRootCAFile": driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":  driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort, Delay: 100 * time.Millisecond}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			driver.Get(params.Ports.ClientPort, "hello, world!"),
			&driver.Fork{
				Fore: &driver.Scenario{
					Steps: []driver.Step{
						&driver.Sleep{Duration: 1 * time.Second},
						&driver.Repeat{
							Duration: 19 * time.Second,
							Step:     driver.Get(params.Ports.ClientPort, "hello, world!"),
						},
					},
				},
				Back: &driver.Repeat{
					Duration: 20 * time.Second,
					Step: &driver.Scenario{
						Steps: []driver.Step{
							&driver.Update{Node: "client", Version: "{{.N}}", Listeners: []string{
								driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
							}},
							&driver.Update{Node: "server", Version: "{{.N}}", Listeners: []string{
								driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
							}},
							// may need short delay so we don't eat all the CPU
							&driver.Sleep{Duration: 100 * time.Millisecond},
						},
					},
				},
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func getSdLogEntries(noClientLogs bool, logEntryCount int) []SDLogEntry {
	logEntries := []SDLogEntry{
		{
			LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
			LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
			LogEntryCount: logEntryCount,
		},
	}

	if !noClientLogs {
		logEntries = append(logEntries, SDLogEntry{
			LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
			LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
			LogEntryCount: logEntryCount,
		})
	}

	return logEntries
}

func TestStackdriverAccessLog(t *testing.T) {
	t.Parallel()
	TestCases := []struct {
		name                   string
		logWindowDuration      string
		sleepDuration          time.Duration
		respCode               string
		logEntryCount          int
		justSendErrorClientLog string
		enableMetadataExchange bool
		sourceUnknown          string
		destinationUnknown     string
	}{
		{"StackdriverAndAccessLogPlugin", "15s", 0, "200", 1, "", true, "", ""},
		{"RequestGetsLoggedAgain", "1s", 1 * time.Second, "200", 2, "", true, "", ""},
		{"AllErrorRequestsGetsLogged", "1s", 0, "403", 10, "", true, "", ""},
		{"AllClientErrorRequestsGetsLoggedOnNoMxAndError", "1s", 0, "403", 10, "true", false, "true", "true"},
		{"NoClientRequestsGetsLoggedOnErrorConfigAndAllSuccessRequests", "15s", 0, "200", 1, "true", false, "true", "true"},
	}

	for _, tt := range TestCases {
		t.Run(tt.name, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"LogWindowDuration":           tt.logWindowDuration,
				"ServiceAuthenticationPolicy": "NONE",
				"DirectResponseCode":          tt.respCode,
				"SDLogStatusCode":             tt.respCode,
				"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
				"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
				"JustSendErrorClientLog":      tt.justSendErrorClientLog,
				"DestinationUnknown":          tt.destinationUnknown,
				"SourceUnknown":               tt.sourceUnknown,
				"LogSampled":                  "true",
			}, envoye2e.ProxyE2ETests)

			sdPort := params.Ports.Max + 1
			stsPort := params.Ports.Max + 2
			params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
			params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
			params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/access_log_policy.yaml.tmpl") + "\n" +
				driver.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
			params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/access_log_policy.yaml.tmpl") + "\n" +
				driver.LoadTestData("testdata/filters/stackdriver_outbound.yaml.tmpl")
			if tt.enableMetadataExchange {
				params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl") + "\n" +
					params.Vars["ServerHTTPFilters"]
				params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl") + "\n" +
					params.Vars["ClientHTTPFilters"]
			}

			sd := &Stackdriver{Port: sdPort}
			respCode, _ := strconv.Atoi(tt.respCode)
			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					sd,
					&SecureTokenService{Port: stsPort},
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
						nil, getSdLogEntries(tt.justSendErrorClientLog == "true" && tt.respCode == "200", tt.logEntryCount),
						true,
					),
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestStackdriverTCPMetadataExchange(t *testing.T) {
	t.Parallel()
	TestCases := []struct {
		name               string
		alpnProtocol       string
		sourceUnknown      string
		destinationUnknown string
	}{
		{"BaseCase", "mx-protocol", "", ""},
		{"NoAlpn", "some-protocol", "true", "true"},
	}

	for _, tt := range TestCases {
		t.Run(tt.name, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"ServiceAuthenticationPolicy": "MUTUAL_TLS",
				"SDLogStatusCode":             "200",
				"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
				"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
				"SourcePrincipal":             "spiffe://cluster.local/ns/default/sa/client",
				"DestinationPrincipal":        "spiffe://cluster.local/ns/default/sa/server",
				"DisableDirectResponse":       "true",
				"AlpnProtocol":                tt.alpnProtocol,
				"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
				"SourceUnknownOnClose":        tt.sourceUnknown,
				"SourceUnknownOnOpen":         tt.sourceUnknown,
				"DestinationUnknown":          tt.destinationUnknown,
				"SourceUnknown":               tt.sourceUnknown,
			}, envoye2e.ProxyE2ETests)

			sdPort := params.Ports.Max + 1
			stsPort := params.Ports.Max + 2
			params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
			params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
			params.Vars["ServerNetworkFilters"] = params.LoadTestData("testdata/filters/server_mx_network_filter.yaml.tmpl") + "\n" +
				params.LoadTestData("testdata/filters/stackdriver_network_inbound.yaml.tmpl")
			params.Vars["ClientUpstreamFilters"] = params.LoadTestData("testdata/filters/client_mx_network_filter.yaml.tmpl")
			params.Vars["ClientNetworkFilters"] = params.LoadTestData("testdata/filters/stackdriver_network_outbound.yaml.tmpl")
			params.Vars["ClientClusterTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
			params.Vars["ServerListenerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")

			sd := &Stackdriver{Port: sdPort}

			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					sd,
					&SecureTokenService{Port: stsPort},
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
					sd.Check(params,
						[]string{
							"testdata/stackdriver/client_tcp_connection_count.yaml.tmpl",
							"testdata/stackdriver/client_tcp_received_bytes_count.yaml.tmpl",
							"testdata/stackdriver/server_tcp_received_bytes_count.yaml.tmpl",
							"testdata/stackdriver/server_tcp_connection_count.yaml.tmpl",
						},
						[]SDLogEntry{
							{
								LogBaseFile: "testdata/stackdriver/server_access_log.yaml.tmpl",
								LogEntryFile: []string{
									"testdata/stackdriver/server_tcp_access_log_entry_on_open.yaml.tmpl",
									"testdata/stackdriver/server_tcp_access_log_entry.yaml.tmpl",
								},
								LogEntryCount: 10,
							},
							{
								LogBaseFile: "testdata/stackdriver/client_access_log.yaml.tmpl",
								LogEntryFile: []string{
									"testdata/stackdriver/client_tcp_access_log_entry_on_open.yaml.tmpl",
									"testdata/stackdriver/client_tcp_access_log_entry.yaml.tmpl",
								},
								LogEntryCount: 10,
							},
						}, false,
					),
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestStackdriverAuditLog(t *testing.T) {
	t.Parallel()
	respCode := "200"
	logEntryCount := 5

	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"DirectResponseCode":          respCode,
		"SDLogStatusCode":             respCode,
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/rbac_log.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl") + "\n" + driver.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl")
	sd := &Stackdriver{Port: sdPort}
	intRespCode, _ := strconv.Atoi(respCode)
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
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
				N: logEntryCount,
				Step: &driver.HTTPCall{
					Port:         params.Ports.ClientPort,
					ResponseCode: intRespCode,
				},
			},
			sd.Check(params,
				nil, []SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: logEntryCount,
					},
					{
						LogBaseFile:   "testdata/stackdriver/server_audit_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_audit_log_entry.yaml.tmpl"},
						LogEntryCount: logEntryCount,
					},
				}, true,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverAttributeGen(t *testing.T) {
	t.Parallel()
	env.EnsureWasmFiles(t)
	env.SkipTSan(t)
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
		"AttributeGenFilterConfig":    "filename: " + env.GetBazelWorkspaceOrDie() + "/extensions/attributegen.wasm",
		"RequestOperation":            "GetMethod",
	}, envoye2e.ProxyE2ETests)
	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)
	params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/attributegen.yaml.tmpl") + "\n" +
		params.Vars["ServerHTTPFilters"]
	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
			&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
				"type_logging_success_true_envoy_export_call": &driver.ExactStat{Metric: "testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverGenericNode(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/generic_client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/generic_server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/client.yaml.tmpl"),
			}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{
				driver.LoadTestData("testdata/listener/server.yaml.tmpl"),
			}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			sd.Check(params,
				[]string{"testdata/stackdriver/generic_client_request_count.yaml.tmpl", "testdata/stackdriver/generic_server_request_count.yaml.tmpl"},
				nil, true,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverCustomAccessLog(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy":         "NONE",
		"SDLogStatusCode":                     "200",
		"StackdriverRootCAFile":               driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":                driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                         driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
		"StackdriverFilterCustomClientConfig": driver.LoadTestJSON("testdata/stackdriver/client_config_customized.yaml.tmpl"),
		"LogsCustomized":                      "true",
		"UserAgent":                           "chrome",
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{
				N: 10,
				Step: &driver.HTTPCall{
					Port:           params.Ports.ClientPort,
					Body:           "hello, world!",
					RequestHeaders: map[string]string{"User-Agent": "chrome"},
				},
			},
			sd.Check(params,
				nil,
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
			&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
				"type_logging_success_true_envoy_export_call": &driver.ExactStat{Metric: "testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverAccessLogFilter(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"UserAgent":                   "chrome",
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_inbound_logs_filter.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stackdriver_outbound_logs_filter.yaml.tmpl")

	sd := &Stackdriver{Port: sdPort}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{
				N: 1,
				Step: &driver.HTTPCall{
					Port:           params.Ports.ClientPort,
					Body:           "hello, world!",
					RequestHeaders: map[string]string{"User-Agent": "chrome"},
				},
			},
			&driver.Repeat{
				N: 1,
				Step: &driver.HTTPCall{
					Port:           params.Ports.ClientPort,
					Body:           "hello, world!",
					RequestHeaders: map[string]string{"User-Agent": "safari", "x-filter": "filter"},
				},
			},
			sd.Check(params,
				nil,
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 1,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 1,
					},
				}, true,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverRbacAccessDenied(t *testing.T) {
	t.Parallel()
	respCode := "403"
	logEntryCount := 5

	rbacCases := []struct {
		name             string
		rbacDryRunResult string
		rbacDryRunFilter string
	}{
		{
			name:             "ActionBoth",
			rbacDryRunResult: "Denied",
			rbacDryRunFilter: "testdata/filters/rbac_dry_run_action_both.yaml.tmpl",
		},
		{
			name:             "ActionDeny",
			rbacDryRunResult: "Denied",
			rbacDryRunFilter: "testdata/filters/rbac_dry_run_action_deny.yaml.tmpl",
		},
		{
			name:             "ActionAllow",
			rbacDryRunResult: "Allowed",
			rbacDryRunFilter: "testdata/filters/rbac_dry_run_action_allow.yaml.tmpl",
		},
	}
	for _, tc := range rbacCases {
		t.Run(tc.name, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"ServiceAuthenticationPolicy": "NONE",
				"DirectResponseCode":          respCode,
				"SDLogStatusCode":             respCode,
				"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
				"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
				"RbacAccessDenied":            "true",
				"RbacDryRunResult":            tc.rbacDryRunResult,
			}, envoye2e.ProxyE2ETests)

			sdPort := params.Ports.Max + 1
			stsPort := params.Ports.Max + 2
			params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
			params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
			params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl")
			params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl") + "\n" +
				driver.LoadTestData(tc.rbacDryRunFilter) + "\n" +
				driver.LoadTestData("testdata/filters/rbac.yaml.tmpl") + "\n" +
				driver.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
			sd := &Stackdriver{Port: sdPort}
			intRespCode, _ := strconv.Atoi(respCode)
			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					sd,
					&SecureTokenService{Port: stsPort},
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
						N: logEntryCount,
						Step: &driver.HTTPCall{
							Port:         params.Ports.ClientPort,
							ResponseCode: intRespCode,
						},
					},
					sd.Check(params,
						nil, []SDLogEntry{
							{
								LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
								LogEntryFile:  []string{"testdata/stackdriver/server_access_log_entry.yaml.tmpl"},
								LogEntryCount: logEntryCount,
							},
						}, true,
					),
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestStackdriverRbacTCPDryRun(t *testing.T) {
	t.Parallel()
	TestCases := []struct {
		name               string
		alpnProtocol       string
		sourceUnknown      string
		destinationUnknown string
	}{
		{"BaseCase", "mx-protocol", "", ""},
		{"NoAlpn", "some-protocol", "true", "true"},
	}

	for _, tt := range TestCases {
		t.Run(tt.name, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"ServiceAuthenticationPolicy": "MUTUAL_TLS",
				"SDLogStatusCode":             "200",
				"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
				"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
				"SourcePrincipal":             "spiffe://cluster.local/ns/default/sa/client",
				"DestinationPrincipal":        "spiffe://cluster.local/ns/default/sa/server",
				"DisableDirectResponse":       "true",
				"AlpnProtocol":                tt.alpnProtocol,
				"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
				"SourceUnknownOnClose":        tt.sourceUnknown,
				"SourceUnknownOnOpen":         tt.sourceUnknown,
				"DestinationUnknown":          tt.destinationUnknown,
				"SourceUnknown":               tt.sourceUnknown,
				"RbacDryRun":                  "true",
			}, envoye2e.ProxyE2ETests)

			sdPort := params.Ports.Max + 1
			stsPort := params.Ports.Max + 2
			params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
			params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
			params.Vars["ServerNetworkFilters"] = params.LoadTestData("testdata/filters/server_mx_network_filter.yaml.tmpl") + "\n" +
				params.LoadTestData("testdata/filters/rbac_tcp.yaml.tmpl") + "\n" +
				params.LoadTestData("testdata/filters/stackdriver_network_inbound.yaml.tmpl")
			params.Vars["ClientUpstreamFilters"] = params.LoadTestData("testdata/filters/client_mx_network_filter.yaml.tmpl")
			params.Vars["ClientNetworkFilters"] = params.LoadTestData("testdata/filters/stackdriver_network_outbound.yaml.tmpl")
			params.Vars["ClientClusterTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
			params.Vars["ServerListenerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")

			sd := &Stackdriver{Port: sdPort}

			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					sd,
					&SecureTokenService{Port: stsPort},
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
					sd.Check(params,
						[]string{
							"testdata/stackdriver/client_tcp_connection_count.yaml.tmpl",
							"testdata/stackdriver/client_tcp_received_bytes_count.yaml.tmpl",
							"testdata/stackdriver/server_tcp_received_bytes_count.yaml.tmpl",
							"testdata/stackdriver/server_tcp_connection_count.yaml.tmpl",
						},
						[]SDLogEntry{
							{
								LogBaseFile: "testdata/stackdriver/server_access_log.yaml.tmpl",
								LogEntryFile: []string{
									"testdata/stackdriver/server_tcp_access_log_entry_on_open.yaml.tmpl",
									"testdata/stackdriver/server_tcp_access_log_entry.yaml.tmpl",
								},
								LogEntryCount: 10,
							},
							{
								LogBaseFile: "testdata/stackdriver/client_access_log.yaml.tmpl",
								LogEntryFile: []string{
									"testdata/stackdriver/client_tcp_access_log_entry_on_open.yaml.tmpl",
									"testdata/stackdriver/client_tcp_access_log_entry.yaml.tmpl",
								},
								LogEntryCount: 10,
							},
						}, false,
					),
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestStackdriverMetricExpiry(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl")
	params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl") +
		driver.LoadTestData("testdata/filters/stackdriver_inbound.yaml.tmpl")
	sd := &Stackdriver{Port: sdPort}
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
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
				N: 10,
				Step: &driver.HTTPCall{
					Port: params.Ports.ClientPort,
					Body: "hello, world!",
				},
			},
			sd.Check(params,
				[]string{"testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{}, true,
			),
			sd.Reset(),
			&driver.Sleep{Duration: 10 * time.Second},
			// Send request directly to server, which will create several new time series with unknown source.
			// This will also trigger the metrics with known source to be purged.
			&driver.Repeat{
				N: 10,
				Step: &driver.HTTPCall{
					Port: params.Ports.ServerPort,
					Body: "hello, world!",
				},
			},
			// Should only have unknown source metric.
			sd.Check(params,
				[]string{"testdata/stackdriver/server_request_count_source_unknown.yaml.tmpl"},
				[]SDLogEntry{}, true,
			),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func TestStackdriverPayloadUtf8(t *testing.T) {
	t.Parallel()
	params := driver.NewTestParams(t, map[string]string{
		"ServiceAuthenticationPolicy": "NONE",
		"SDLogStatusCode":             "200",
		"StackdriverRootCAFile":       driver.TestPath("testdata/certs/stackdriver.pem"),
		"StackdriverTokenFile":        driver.TestPath("testdata/certs/access-token"),
		"StatsConfig":                 driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
	}, envoye2e.ProxyE2ETests)

	sdPort := params.Ports.Max + 1
	stsPort := params.Ports.Max + 2
	params.Vars["SDPort"] = strconv.Itoa(int(sdPort))
	params.Vars["STSPort"] = strconv.Itoa(int(stsPort))
	params.Vars["ClientMetadata"] = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
	enableStackDriver(t, params.Vars)

	sd := &Stackdriver{Port: sdPort}

	bad := "va\xC0lue"
	get := &driver.HTTPCall{
		Method: "GET",
		Port:   params.Ports.ClientPort,
		Body:   "hello, world!",
		RequestHeaders: map[string]string{
			"referer":                   bad,
			"user-agent":                bad,
			"x-envoy-original-path":     bad,
			"x-envoy-original-dst-host": bad,
			"x-request-id":              bad,
			"x-b3-traceid":              bad,
			"x-b3-spanid":               bad,
		},
	}

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			sd,
			&SecureTokenService{Port: stsPort},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: get},
			sd.Check(params,
				[]string{"testdata/stackdriver/client_request_count.yaml.tmpl", "testdata/stackdriver/server_request_count.yaml.tmpl"},
				[]SDLogEntry{
					{
						LogBaseFile:   "testdata/stackdriver/server_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/utf8_server_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
					{
						LogBaseFile:   "testdata/stackdriver/client_access_log.yaml.tmpl",
						LogEntryFile:  []string{"testdata/stackdriver/utf8_client_access_log_entry.yaml.tmpl"},
						LogEntryCount: 10,
					},
				}, true,
			),
			&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
				"type_logging_success_true_envoy_export_call": &driver.ExactStat{Metric: "testdata/metric/stackdriver_callout_metric.yaml.tmpl"},
			}},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
