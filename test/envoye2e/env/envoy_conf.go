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

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"text/template"
)

const envoyClientConfTemplYAML = `
node:
  id: test-client
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
  - name: server
    connect_timeout: 5s
    type: STATIC
    load_assignment:
      cluster_name: server
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{.Ports.ProxyToServerProxyPort}}
  listeners:
  - name: app-to-client
    traffic_direction: INBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.AppToClientProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          codec_type: AUTO
          stat_prefix: inbound_http
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ClientAccessLogPath}}
          http_filters:
{{.FiltersBeforeEnvoyRouterInAppToClient | indent 10 }}
          - name: envoy.router
          route_config:
            name: app-to-client-route
            virtual_hosts:
            - name: app-to-client-route
              domains: ["*"]
              routes:
              - match:
                  prefix: /
                route:
                  cluster: client
                  timeout: 0s
  - name: client-to-proxy
    traffic_direction: OUTBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToServerProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          codec_type: AUTO
          stat_prefix: outbound_http
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ClientAccessLogPath}}
          http_filters:
{{.FiltersBeforeEnvoyRouterInClientToProxy | indent 10 }}
          - name: envoy.router
          route_config:
            name: client-to-proxy-route
            virtual_hosts:
            - name: client-to-proxy-route
              domains: ["*"]
              routes:
              - match:
                  prefix: /
                route:
                  cluster: server
                  timeout: 0s
`

const envoyServerConfTemplYAML = `
node: 
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
  - name: inbound|9080|http|backend.default.svc.cluster.local
    connect_timeout: 5s
    type: STATIC
    load_assignment:
      cluster_name: inbound|9080|http|backend.default.svc.cluster.local
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{.Ports.BackendPort}}
  - name: inbound|9080|http|server.default.svc.cluster.local
    connect_timeout: 5s
    type: STATIC
    load_assignment:
      cluster_name: inbound|9080|http|server.default.svc.cluster.local
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{.Ports.ClientToAppProxyPort}}
  listeners:
  - name: proxy-to-server
    traffic_direction: INBOUND
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ProxyToServerProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          codec_type: AUTO
          stat_prefix: inbound_http
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
          http_filters:
{{.FiltersBeforeEnvoyRouterInProxyToServer | indent 10 }}
          - name: envoy.router
          route_config:
            name: proxy-to-server-route
            virtual_hosts:
            - name: proxy-to-server-route
              domains: ["*"]
              routes:
              - match:
                  prefix: /
                route:
                  cluster: inbound|9080|http|server.default.svc.cluster.local
                  timeout: 0s
  - name: client-to-app
    address:
      socket_address:
        address: 127.0.0.1
        port_value: {{.Ports.ClientToAppProxyPort}}
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          codec_type: AUTO
          stat_prefix: outbound_http
          access_log:
          - name: envoy.file_access_log
            config:
              path: {{.ServerAccessLogPath}}
          http_filters:
{{.FiltersBeforeEnvoyRouterInClientToApp | indent 10 }}
          - name: envoy.router
          route_config:
            name: client-to-app-route
            virtual_hosts:
            - name: client-to-app-route
              domains: ["*"]
              routes:
              - match:
                  prefix: /
                route:
                  cluster: inbound|9080|http|backend.default.svc.cluster.local
                  timeout: 0s
`

// CreateEnvoyConf create envoy config.
func (s *TestSetup) CreateEnvoyConf(path, confTmpl string) error {
	if s.stress {
		s.AccessLogPath = "/dev/null"
	}

	tmpl, err := template.New("test").Funcs(template.FuncMap{
		"indent": indent,
	}).Parse(confTmpl)
	if err != nil {
		return fmt.Errorf("failed to parse config template: %v", err)
	}
	tmpl.Funcs(template.FuncMap{})

	err = os.MkdirAll(filepath.Dir(path), os.ModePerm)
	if err != nil {
		return fmt.Errorf("failed to create dir %v: %v", filepath.Dir(path), err)
	}
	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("failed to create file %v: %v", path, err)
	}
	defer func() {
		_ = f.Close()
	}()

	return tmpl.Execute(f, s)
}

func indent(n int, s string) string {
	pad := strings.Repeat(" ", n)
	return pad + strings.Replace(s, "\n", "\n"+pad, -1)
}
