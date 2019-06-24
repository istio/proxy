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
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
  - name: server
    connect_timeout: 5s
    type: STATIC
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ProxyToServerProxyPort}}
  listeners:
  - name: app-to-client
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.AppToClientProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.tcp_proxy
        config:
          stat_prefix: inbound_tcp
          cluster: client
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ClientAccessLogPath}}
  - name: client-to-proxy
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.tcp_proxy
        config:
          stat_prefix: outbound_tcp
          cluster: server
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ClientAccessLogPath}}
`

const tcpEnvoyServerConfTemplYAML = `
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
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.BackendPort}}
  - name: server
    connect_timeout: 5s
    type: STATIC
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToAppProxyPort}}
  listeners:
  - name: proxy-to-server
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ProxyToServerProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.tcp_proxy
        config:
          stat_prefix: inbound_tcp
          cluster: server
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
  - name: client-to-app
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToAppProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.tcp_proxy
        config:
          stat_prefix: outbound_tcp
          cluster: backend
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
`

func GetTcpClientEnvoyConfTmp() string {
	return tcpEnvoyClientConfTemplYAML
}

func GetTcpServerEnvoyConfTmp() string {
	return tcpEnvoyServerConfTemplYAML
}
