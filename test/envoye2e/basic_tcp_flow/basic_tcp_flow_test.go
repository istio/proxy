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
	"bufio"
	"bytes"
	"fmt"
	"net"
	"testing"
	"text/template"

	"istio.io/proxy/test/envoye2e/env"
)

// Stats in Client Envoy proxy.
var expectedClientStats = map[string]int{
	// tcp listener stats
	"tcp.inbound_tcp.downstream_cx_total": 1,
}

// Stats in Server Envoy proxy.
var expectedServerStats = map[string]int{
	// tcp listener stats
	"tcp.outbound_tcp.downstream_cx_total": 1,
}

func TestTcpBasicFlow(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.BasicTCPFlowTest, t)
	s.SetNoBackend(true)
	s.SetStartTcpBackend(true)
	s.ClientEnvoyTemplate = env.GetTcpClientEnvoyConfTmp()
	s.ServerEnvoyTemplate = env.GetTcpServerEnvoyConfTmp()
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	conn, _ := net.Dial("tcp", fmt.Sprintf("localhost:%d", s.Ports().AppToClientProxyPort))
	// send to socket
	fmt.Fprintf(conn, "world"+"\n")
	// listen for reply
	message, _ := bufio.NewReader(conn).ReadString('\n')

	if message != "hello world\n" {
		t.Fatalf("Verification Failed. Expected: hello world. Got: %v", message)
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
