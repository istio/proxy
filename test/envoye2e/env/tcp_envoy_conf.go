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

package env

const tcpEnvoyClientConfTemplYAML = `
node:
  id: test
  metadata: {
{{.ClientNodeMetadata | indent 4 }}
  }
{{.ExtraConfig }}
admin:
  access_log_path: {{.ClientAccessLogPath}}
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{.Ports.ClientAdminPort}}
static_resources:
  clusters:
  - name: client
    connect_timeout: 5s
    type: STATIC
    load_assignment:
      cluster_name: client
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{.Ports.ClientToServerProxyPort}}
{{.UpstreamFiltersInClient | indent 4 }}    
{{.ClusterTLSContext | indent 4 }}
  listeners:
  - name: app-to-client
    traffic_direction: OUTBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.AppToClientProxyPort}}
    listener_filters:
    - name: "envoy.filters.listener.tls_inspector"
    - name: "envoy.filters.listener.http_inspector"
    filter_chains:
    - filters:
{{.FiltersBeforeEnvoyRouterInAppToClient | indent 6 }}
      - name: tcp_proxy
        typed_config:
          "@type": type.googleapis.com/envoy.config.filter.network.tcp_proxy.v2.TcpProxy
          stat_prefix: inbound_tcp
          cluster: client
          access_log:
          - name: log
            typed_config:
              "@type": type.googleapis.com/envoy.config.accesslog.v2.FileAccessLog
              path: {{.ClientAccessLogPath}}
              format: {{.AccesslogFormat}}
{{.TLSContext | indent 6 }}
`

const tcpEnvoyServerConfTemplYAML = `
node:
  id: test
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
  - name: backend
    connect_timeout: 5s
    type: STATIC
    load_assignment:
      cluster_name: backend
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{.Ports.BackendPort}}
{{.ServerClusterTLSContext | indent 4 }}
  listeners:
  - name: server
    traffic_direction: INBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
    listener_filters:
    - name: "envoy.filters.listener.tls_inspector"
    - name: "envoy.filters.listener.http_inspector"
    filter_chains:
    - filters:
{{.FiltersBeforeEnvoyRouterInProxyToServer | indent 6 }}
      - name: tcp_proxy
        typed_config:
          "@type": type.googleapis.com/envoy.config.filter.network.tcp_proxy.v2.TcpProxy
          stat_prefix: outbound_tcp
          cluster: backend
          access_log:
          - name: log
            typed_config:
              "@type": type.googleapis.com/envoy.config.accesslog.v2.FileAccessLog
              path: {{.ServerAccessLogPath}}
              format: {{.ServerAccesslogFormat}}
{{.ServerTLSContext | indent 6 }}
`

func GetTCPClientEnvoyConfTmp() string {
	return tcpEnvoyClientConfTemplYAML
}

func GetTCPServerEnvoyConfTmp() string {
	return tcpEnvoyServerConfTemplYAML
}
