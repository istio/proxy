// Copyright 2021 Istio Authors
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

package ratelimit_test

import (
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
)

// TestHTTPLocalRatelimit validates that envoy can rate limit based on:
// - source attribute, produced by MX extension
// - request header
func TestHTTPLocalRatelimit(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/mx_native_outbound.yaml.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/mx_native_inbound.yaml.tmpl") + "\n" +
		params.LoadTestData("testdata/filters/local_ratelimit_inbound.yaml.tmpl")
	params.Vars["ServerRouteRateLimits"] = `
rate_limits:
- actions:
  - request_headers:
      header_name: user-id
      descriptor_key: id
  - extension:
      name: custom
      typed_config:
        "@type": type.googleapis.com/udpa.type.v1.TypedStruct
        type_url: type.googleapis.com/envoy.extensions.rate_limit_descriptors.expr.v3.Descriptor
        value:
          descriptor_key: app
          text: filter_state['wasm.downstream_peer'].labels['app'].value`
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "client", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/client.yaml.tmpl")}},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.HTTPCall{
				Port: params.Ports.ClientPort,
				Body: "hello, world!",
			},
			// Only first call should pass with 1req/minute
			&driver.HTTPCall{
				Port:           params.Ports.ClientPort,
				RequestHeaders: map[string]string{"user-id": "foo"},
				ResponseCode:   200,
			},
			&driver.HTTPCall{
				Port:           params.Ports.ClientPort,
				RequestHeaders: map[string]string{"user-id": "foo"},
				ResponseCode:   429,
				Body:           "local_rate_limited",
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
