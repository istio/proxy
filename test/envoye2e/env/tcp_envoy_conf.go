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
{{.ClusterTlsContext | indent 4 }}
  listeners:
  - name: app-to-client
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.AppToClientProxyPort}}
    listener_filters:
    - name: "envoy.listener.tls_inspector"
      typed_config: {}
    - name: "envoy.listener.http_inspector"
      typed_config: {}
    filter_chains:
    - filters:
{{.FiltersBeforeEnvoyRouterInAppToClient | indent 6 }}
      - name: envoy.tcp_proxy
        config:
          stat_prefix: inbound_tcp
          cluster: client
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ClientAccessLogPath}}
              format: {{.AccesslogFormat}}
{{.TlsContext | indent 6 }}
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
{{.ClusterTlsContext | indent 4 }}
  listeners:
  - name: server
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
    listener_filters:
    - name: "envoy.listener.tls_inspector"
      typed_config: {}
    - name: "envoy.listener.http_inspector"
      typed_config: {}
    filter_chains:
    - filters:
{{.FiltersBeforeEnvoyRouterInClientToApp | indent 6 }}
      - name: envoy.tcp_proxy
        config:
          stat_prefix: outbound_tcp
          cluster: backend
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
              format: {{.ServerAccesslogFormat}}
{{.TlsContext | indent 6 }}
`

func GetTcpClientEnvoyConfTmp() string {
	return tcpEnvoyClientConfTemplYAML
}

func GetTcpServerEnvoyConfTmp() string {
	return tcpEnvoyServerConfTemplYAML
}
