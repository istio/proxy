// Copyright 2020 Istio Authors
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
	"encoding/base64"
	"testing"
	"time"

	"github.com/golang/protobuf/proto"
	pstruct "github.com/golang/protobuf/ptypes/struct"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
)

const ServerHTTPListener = `
name: server
traffic_direction: INBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Ports.ServerPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: server
      http_filters:
      - name: envoy.filters.http.wasm
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: envoy.extensions.filters.http.wasm.v3.Wasm
          value:
            config:
              vm_config:
                runtime: envoy.wasm.runtime.null
                code:
                  local:
                    inline_string: envoy.wasm.metadata_exchange
              configuration: "{ max_peer_cache_size: 20 }"
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
`

func EncodeMetadata(t *testing.T, p *driver.Params) string {
	pb := &pstruct.Struct{}
	err := p.FillYAML("{"+p.Vars["ClientMetadata"]+"}", pb)
	if err != nil {
		t.Fatal(err)
	}
	bytes, err := proto.Marshal(pb)
	if err != nil {
		t.Fatal(err)
	}
	return base64.RawStdEncoding.EncodeToString(bytes)
}

func TestHTTPExchange(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	if err := (&driver.Scenario{
		[]driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{ServerHTTPListener}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{1 * time.Second},
			&driver.HTTPCall{
				Port: params.Ports.ServerPort,
				Body: "hello, world!",
				ResponseHeaders: map[string]string{
					"x-envoy-peer-metadata-id": driver.None,
					"x-envoy-peer-metadata":    driver.None,
				},
			},
			&driver.HTTPCall{
				Port: params.Ports.ServerPort,
				Body: "hello, world!",
				RequestHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "client",
				},
				ResponseHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "server",
					"x-envoy-peer-metadata":    driver.None,
				},
			},
			&driver.HTTPCall{
				Port: params.Ports.ServerPort,
				Body: "hello, world!",
				RequestHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "client",
					"x-envoy-peer-metadata":    EncodeMetadata(t, params),
				},
				ResponseHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "server",
					"x-envoy-peer-metadata":    driver.Any,
				},
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
