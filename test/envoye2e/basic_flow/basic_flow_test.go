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
	"text/template"

	"bytes"

	"istio.io/proxy/test/envoye2e/env"
)

// Stats in Client Envoy proxy.
var expectedClientStats = map[string]int{
	// http listener stats
	"listener.127.0.0.1_{{.Ports.AppToClientProxyPort}}.http.inbound_http.downstream_rq_completed":     10,
	"listener.127.0.0.1_{{.Ports.AppToClientProxyPort}}.http.inbound_http.downstream_rq_2xx":           10,
	"listener.127.0.0.1_{{.Ports.ClientToServerProxyPort}}.http.outbound_http.downstream_rq_completed": 10,
	"listener.127.0.0.1_{{.Ports.ClientToServerProxyPort}}.http.outbound_http.downstream_rq_2xx":       10,
}

// Stats in Server Envoy proxy.
var expectedServerStats = map[string]int{
	// http listener stats
	"listener.127.0.0.1_{{.Ports.ProxyToServerProxyPort}}.http.inbound_http.downstream_rq_completed": 10,
	"listener.127.0.0.1_{{.Ports.ProxyToServerProxyPort}}.http.inbound_http.downstream_rq_2xx":       10,
	"listener.127.0.0.1_{{.Ports.ClientToAppProxyPort}}.http.outbound_http.downstream_rq_completed":  10,
	"listener.127.0.0.1_{{.Ports.ClientToAppProxyPort}}.http.outbound_http.downstream_rq_2xx":        10,
}

func TestBasicFlow(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.BasicFlowTest, t)
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	url := fmt.Sprintf("http://localhost:%d/echo", s.Ports().AppToClientProxyPort)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	for i := 0; i < 10; i++ {
		if _, _, err := env.HTTPGet(url); err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
	}

	s.VerifyEnvoyStats(getParsedExpectedStats(expectedClientStats, t, s), s.Ports().ClientAdminPort)
	s.VerifyEnvoyStats(getParsedExpectedStats(expectedServerStats, t, s), s.Ports().ServerAdminPort)
}

func getParsedExpectedStats(expectedStats map[string]int, t *testing.T, s *env.TestSetup) map[string]int {
	parsedExpectedStats := make(map[string]int)
	for key, value := range expectedStats {
		tmpl, err := template.New("parse_state").Parse(key)
		if err != nil {
			t.Errorf("failed to parse config template: %v", err)
		}

		var tpl bytes.Buffer
		err = tmpl.Execute(&tpl, s)
		if err != nil {
			t.Errorf("failed to execute config template: %v", err)
		}
		parsedExpectedStats[tpl.String()] = value
	}

	return parsedExpectedStats
}
