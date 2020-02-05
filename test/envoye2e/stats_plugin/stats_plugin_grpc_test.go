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
	"context"
	"fmt"
	"testing"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"istio.io/proxy/test/envoye2e/env"
	"istio.io/proxy/test/envoye2e/env/grpc_echo"
)

const grpcEnvoyServerTemplate = `node:
  id: test-server
  metadata: {
{{.ServerNodeMetadata | indent 4 }}
  }
{{.ExtraConfig }}
admin:
  access_log_path: {{.ServerAccessLogPath}}
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{.Ports.ServerAdminPort}}
static_resources:
  clusters:
  - name: inbound|9080|grpc|server.default.svc.cluster.local
    connect_timeout: 5s
    type: STATIC
    http2_protocol_options: {}
    load_assignment:
      cluster_name: inbound|9080|grpc|server.default.svc.cluster.local
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{.Ports.BackendPort}}
{{.ClusterTLSContext | indent 4 }}
  listeners:
  - name: proxy-to-backend
    traffic_direction: INBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
    filter_chains:
    - filters:
{{.FiltersBeforeHTTPConnectionManagerInProxyToServer | indent 6 }}
      - name: envoy.http_connection_manager
        config:
          stat_prefix: inbound_http
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
          http_filters:
{{.FiltersBeforeEnvoyRouterInProxyToServer | indent 10 }}
          - name: envoy.router
          route_config:
            name: proxy-to-backend-route
            virtual_hosts:
            - name: proxy-to-backend-route
              domains: ["*"]
              routes:
              - match:
                  prefix: /
                route:
                  cluster: inbound|9080|http|server.default.svc.cluster.local
                  timeout: 0s
{{.TLSContext | indent 6 }}
  listeners:
  - name: proxy-to-backend
    traffic_direction: INBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
    filter_chains:
    - filters:
{{.FiltersBeforeHTTPConnectionManagerInProxyToServer | indent 6 }}
      - name: envoy.http_connection_manager
        config:
          stat_prefix: inbound_grpc
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
          http2_protocol_options: {}
          http_filters:
{{.FiltersBeforeEnvoyRouterInProxyToServer | indent 10 }}
          - name: envoy.router
          route_config:
            name: proxy-to-backend-route
            virtual_hosts:
            - name: proxy-to-backend-route
              domains: ["*"]
              routes:
              - match:
                  prefix: /
                route:
                  cluster: inbound|9080|grpc|server.default.svc.cluster.local
                  timeout: 0s
                  max_grpc_timeout: 0s
{{.TLSContext | indent 6 }}`

func TestStatsPluginGRPC(t *testing.T) {
	testStatsPluginGRPC(t, func(s *env.TestSetup) {
		svrStats := map[string]env.Stat{
			"istio_requests_total": {Value: 10, Labels: map[string]string{"grpc_response_status": "7"}},
		}
		s.VerifyPrometheusStats(svrStats, s.Ports().ServerAdminPort)
		clntStats := map[string]env.Stat{
			"istio_requests_total": {
				Value: 10, Labels: map[string]string{
					"destination_service":  fmt.Sprintf("127.0.0.1:%d", s.Ports().AppToClientProxyPort),
					"grpc_response_status": "7",
				}},
		}
		s.VerifyPrometheusStats(clntStats, s.Ports().ClientAdminPort)
	})
}

func testStatsPluginGRPC(t *testing.T, fn verifyFn) {
	s := env.NewClientServerEnvoyTestSetup(env.StatsPluginTest, t)
	s.SetStartHTTPBackend(false)
	s.SetStartGRPCBackend(true)
	s.SetFiltersBeforeEnvoyRouterInAppToClient(fmt.Sprintf(outboundStatsFilter, false))
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(inboundStatsFilter)
	s.SetServerNodeMetadata(inboundNodeMetadata)
	s.SetClientNodeMetadata(outboundNodeMetadata)
	s.SetExtraConfig(statsConfig)
	s.ServerEnvoyTemplate = grpcEnvoyServerTemplate
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	proxyAddr := fmt.Sprintf("127.0.0.1:%d", s.Ports().AppToClientProxyPort)
	conn, err := grpc.Dial(proxyAddr, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		t.Fatalf("Could not establish client connection to gRPC server: %v", err)
	}
	defer conn.Close()
	client := grpc_echo.NewEchoClient(conn)

	for i := 0; i < 10; i++ {
		_, grpcErr := client.Echo(context.Background(), &grpc_echo.EchoRequest{ReturnStatus: status.New(codes.PermissionDenied, "denied").Proto()})
		if fromErr, ok := status.FromError(grpcErr); ok && fromErr.Code() != codes.PermissionDenied {
			t.Logf("Failed GRPC call: %#v (code: %v)", grpcErr, fromErr.Code())
		}
	}

	fn(s)
}
