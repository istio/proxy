// Copyright Istio Authors
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

package otel

import (
	"strconv"
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
)

func TestOtelPayload(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	port := params.Ports.Max
	params.Vars["ClientMetadata"] = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_inbound.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stats_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = driver.LoadTestData("testdata/filters/mx_outbound.yaml.tmpl") + "\n" +
		driver.LoadTestData("testdata/filters/stats_outbound.yaml.tmpl")
	params.Vars["StatsConfig"] = driver.LoadTestData("testdata/bootstrap/otel_stats.yaml.tmpl")
	params.Vars["OtelPort"] = strconv.Itoa(int(port))
	params.Vars["ClientStaticCluster"] = params.LoadTestData("testdata/cluster/otel.yaml.tmpl")
	params.Vars["ServerStaticCluster"] = params.Vars["ClientStaticCluster"]
	params.Vars["ClientHTTPAccessLogs"] = params.LoadTestData("testdata/access_logs/otel.yaml.tmpl")
	params.Vars["ServerHTTPAccessLogs"] = params.LoadTestData("testdata/access_logs/otel.yaml.tmpl")

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Otel{Port: port},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.Repeat{N: 10, Step: driver.Get(params.Ports.ClientPort, "hello, world!")},
			&driver.Sleep{Duration: 10 * time.Second},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
