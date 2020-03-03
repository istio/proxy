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
	"bytes"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"math/rand"
	"sync"
	"time"

	"testing"
	"text/template"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

const metadataExchangeIstioConfigFilter = `
- name: envoy.filters.network.metadata_exchange
  config:
    protocol: istio2
- name: envoy.filters.network.wasm
  config:
    config:
      root_id: "stats_inbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          local: { inline_string: "envoy.wasm.stats" }
      configuration: |
        { "debug": "false", max_peer_cache_size: 20, field_separator: ";.;", tcp_reporting_duration: "0.00000001s" }
`

var (
	statsConfig        = driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl")
	clientNodeMetadata = driver.LoadTestData("testdata/client_node_metadata.json.tmpl")
	serverNodeMetadata = driver.LoadTestData("testdata/server_node_metadata.json.tmpl")
)

const metadataExchangeIstioUpstreamConfigFilterChain = `
filters:
- name: envoy.filters.network.upstream.metadata_exchange
  typed_config: 
    "@type": type.googleapis.com/envoy.tcp.metadataexchange.config.MetadataExchange
    protocol: istio2
`

const metadataExchangeIstioClientFilter = `
- name: envoy.filters.network.wasm
  config:
    config:
      root_id: "stats_outbound"
      vm_config:
        runtime: envoy.wasm.runtime.null
        code:
          local: { inline_string: "envoy.wasm.stats" }
      configuration: |
        { "debug": "false", max_peer_cache_size: 20, field_separator: ";.;", tcp_reporting_duration: "0.1s" }
`

const tlsContext = `
tls_context:
  common_tls_context:
    alpn_protocols:
    - istio2
    tls_certificates:
    - certificate_chain: { filename: "testdata/certs/cert-chain.pem" }
      private_key: { filename: "testdata/certs/key.pem" }
    validation_context:
      trusted_ca: { filename: "testdata/certs/root-cert.pem" }
  require_client_certificate: true
`

const clusterTLSContext = `
tls_context:
  common_tls_context:
    alpn_protocols:
    - istio2
    tls_certificates:
    - certificate_chain: { filename: "testdata/certs/cert-chain.pem" }
      private_key: { filename: "testdata/certs/key.pem" }
    validation_context:
      trusted_ca: { filename: "testdata/certs/root-cert.pem" }
`

const serverTLSContext = `
tls_context:
  common_tls_context:
    alpn_protocols:
    - istio3
    tls_certificates:
    - certificate_chain: { filename: "testdata/certs/cert-chain.pem" }
      private_key: { filename: "testdata/certs/key.pem" }
    validation_context:
      trusted_ca: { filename: "testdata/certs/root-cert.pem" }
  require_client_certificate: true
`

const serverClusterTLSContext = `
tls_context:
  common_tls_context:
    alpn_protocols:
    - istio3
    tls_certificates:
    - certificate_chain: { filename: "testdata/certs/cert-chain.pem" }
      private_key: { filename: "testdata/certs/key.pem" }
    validation_context:
      trusted_ca: { filename: "testdata/certs/root-cert.pem" }
`

// Stats in Client Envoy proxy.
var expectedClientStats = map[string]int{
	"cluster.client.metadata_exchange.alpn_protocol_found":      5,
	"cluster.client.metadata_exchange.alpn_protocol_not_found":  0,
	"cluster.client.metadata_exchange.initial_header_not_found": 0,
	"cluster.client.metadata_exchange.header_not_found":         0,
	"cluster.client.metadata_exchange.metadata_added":           5,
}

// Stats in Server Envoy proxy.
var expectedPrometheusServerLabels = map[string]string{
	"reporter":        "destination",
	"source_app":      "productpage",
	"destination_app": "ratings",
}
var expectedPrometheusServerStats = map[string]env.Stat{
	"istio_tcp_connections_opened_total": {Value: 5, Labels: expectedPrometheusServerLabels},
	"istio_tcp_connections_closed_total": {Value: 5, Labels: expectedPrometheusServerLabels},
	"istio_tcp_received_bytes_total":     {Value: 35, Labels: expectedPrometheusServerLabels},
	"istio_tcp_sent_bytes_total":         {Value: 65, Labels: expectedPrometheusServerLabels},
}

// Stats in Server Envoy proxy Fail Case.
var expectedPrometheusServerLabelsFailCase = map[string]string{
	"reporter":        "destination",
	"source_app":      "unknown",
	"destination_app": "ratings",
}
var expectedPrometheusServerStatsFailCase = map[string]env.Stat{
	"istio_tcp_connections_opened_total": {Value: 5, Labels: expectedPrometheusServerLabelsFailCase},
	"istio_tcp_connections_closed_total": {Value: 5, Labels: expectedPrometheusServerLabelsFailCase},
	"istio_tcp_received_bytes_total":     {Value: 35, Labels: expectedPrometheusServerLabelsFailCase},
	"istio_tcp_sent_bytes_total":         {Value: 65, Labels: expectedPrometheusServerLabelsFailCase},
}

// Stats in Client Envoy proxy.
var expectedPrometheusClientLabels = map[string]string{
	"reporter":        "source",
	"source_app":      "productpage",
	"destination_app": "ratings",
}
var expectedPrometheusClientStats = map[string]env.Stat{
	"istio_tcp_connections_opened_total": {Value: 5, Labels: expectedPrometheusClientLabels},
	"istio_tcp_connections_closed_total": {Value: 5, Labels: expectedPrometheusClientLabels},
	"istio_tcp_received_bytes_total":     {Value: 35, Labels: expectedPrometheusClientLabels},
	"istio_tcp_sent_bytes_total":         {Value: 65, Labels: expectedPrometheusClientLabels},
}

// Stats in Server Envoy proxy.
var expectedServerStats = map[string]int{
	"metadata_exchange.alpn_protocol_found":      5,
	"metadata_exchange.alpn_protocol_not_found":  0,
	"metadata_exchange.initial_header_not_found": 0,
	"metadata_exchange.header_not_found":         0,
	"metadata_exchange.metadata_added":           5,
}

var expectedServerStatsFailCase = map[string]int{
	"metadata_exchange.alpn_protocol_found":      0,
	"metadata_exchange.alpn_protocol_not_found":  5,
	"metadata_exchange.initial_header_not_found": 0,
	"metadata_exchange.header_not_found":         0,
	"metadata_exchange.metadata_added":           0,
}

func TestTCPMetadataExchange(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.TCPMetadataExchangeTest, t)
	s.Dir = driver.BazelWorkspace()
	s.SetStartHTTPBackend(false)
	s.SetStartTCPBackend(true)
	s.SetTLSContext(tlsContext)
	s.SetClusterTLSContext(clusterTLSContext)
	s.SetServerTLSContext(tlsContext)
	s.SetServerClusterTLSContext(clusterTLSContext)
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(metadataExchangeIstioConfigFilter)
	s.SetUpstreamFiltersInClient(metadataExchangeIstioUpstreamConfigFilterChain)
	s.SetFiltersBeforeEnvoyRouterInAppToClient(metadataExchangeIstioClientFilter)
	s.SetEnableTLS(true)
	s.SetClientNodeMetadata(clientNodeMetadata)
	s.SetServerNodeMetadata(serverNodeMetadata)
	s.SetExtraConfig(statsConfig)
	s.ClientEnvoyTemplate = env.GetTCPClientEnvoyConfTmp()
	s.ServerEnvoyTemplate = env.GetTCPServerEnvoyConfTmp()
	s.SetCopyYamlFiles(true)
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup te1	st: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	SendRequests(t, s)

	s.VerifyEnvoyStats(getParsedExpectedStats(expectedClientStats, t, s), s.Ports().ClientAdminPort)
	s.VerifyEnvoyStats(getParsedExpectedStats(expectedServerStats, t, s), s.Ports().ServerAdminPort)

	time.Sleep(time.Second * 5)
	s.VerifyPrometheusStats(expectedPrometheusServerStats, s.Ports().ServerAdminPort)
	s.VerifyPrometheusStats(expectedPrometheusClientStats, s.Ports().ClientAdminPort)
}

func TestTCPMetadataExchangeNoClientFilter(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.TCPMetadataExchangeFailTest, t)
	s.Dir = driver.BazelWorkspace()
	s.SetStartHTTPBackend(false)
	s.SetStartTCPBackend(true)
	// Client send istio2  alpn in tls context.
	s.SetTLSContext(tlsContext)
	s.SetClusterTLSContext(clusterTLSContext)
	// Server accepts istio3 alpn in tls context.
	s.SetServerTLSContext(serverTLSContext)
	s.SetServerClusterTLSContext(serverClusterTLSContext)
	// Only setting mxc filter in server and stats filter in client and filter.
	// Mxc  upstream filter in  server is not set.
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(metadataExchangeIstioConfigFilter)
	s.SetFiltersBeforeEnvoyRouterInAppToClient(metadataExchangeIstioClientFilter)
	s.SetEnableTLS(true)
	s.SetClientNodeMetadata(clientNodeMetadata)
	s.SetServerNodeMetadata(serverNodeMetadata)
	s.SetExtraConfig(statsConfig)
	s.ClientEnvoyTemplate = env.GetTCPClientEnvoyConfTmp()
	s.ServerEnvoyTemplate = env.GetTCPServerEnvoyConfTmp()
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup te1	st: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	SendRequests(t, s)
	s.VerifyEnvoyStats(getParsedExpectedStats(expectedServerStatsFailCase, t, s), s.Ports().ServerAdminPort)

	time.Sleep(time.Second * 5)
	s.VerifyPrometheusStats(expectedPrometheusServerStatsFailCase, s.Ports().ServerAdminPort)
}

func SendRequests(t *testing.T, s *env.TestSetup) {
	certPool := x509.NewCertPool()
	bs, err := ioutil.ReadFile(driver.TestPath("testdata/certs/cert-chain.pem"))
	if err != nil {
		t.Fatalf("failed to read client ca cert: %s", err)
	}
	ok := certPool.AppendCertsFromPEM(bs)
	if !ok {
		t.Fatal("failed to append client certs")
	}

	certificate, err := tls.LoadX509KeyPair(driver.TestPath("testdata/certs/cert-chain.pem"),
		driver.TestPath("testdata/certs/key.pem"))
	if err != nil {
		t.Fatal("failed to get certificate")
	}
	config := &tls.Config{Certificates: []tls.Certificate{certificate}, ServerName: "localhost", NextProtos: []string{"istio3"}, RootCAs: certPool}
	rand.Seed(time.Now().UTC().UnixNano())

	var wg sync.WaitGroup

	response := make(chan error, 5)
	wg.Add(5)
	go sendRequest(response, config, s.Ports().AppToClientProxyPort, &wg)
	go sendRequest(response, config, s.Ports().AppToClientProxyPort, &wg)
	go sendRequest(response, config, s.Ports().AppToClientProxyPort, &wg)
	go sendRequest(response, config, s.Ports().AppToClientProxyPort, &wg)
	go sendRequest(response, config, s.Ports().AppToClientProxyPort, &wg)
	wg.Wait()
	close(response)

	for err := range response {
		if err != nil {
			t.Fatal(err)
		}
	}
}

func sendRequest(response chan<- error, config *tls.Config, port uint16, wg *sync.WaitGroup) {
	defer wg.Done()
	conn, err := tls.Dial("tcp", fmt.Sprintf("localhost:%d", port), config)
	if err != nil {
		response <- err
	}

	for i := 1; i <= 30; i++ {
		conn.Write([]byte("world \n"))
		reply := make([]byte, 256)
		n, err := conn.Read(reply)
		if err != nil {
			response <- err
			break
		}

		if fmt.Sprintf("%s", reply[:n]) != "hello world \n" {
			response <- fmt.Errorf("verification Failed. Expected: hello world. Got: %v", fmt.Sprintf("%s", reply[:n]))
			break
		}
		time.Sleep(time.Second * 1)
	}

	_ = conn.Close()

	response <- nil
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
