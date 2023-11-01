// Code generated for package testdata by go-bindata DO NOT EDIT. (@generated)
// sources:
// bootstrap/client.yaml.tmpl
// bootstrap/otel_stats.yaml.tmpl
// bootstrap/server.yaml.tmpl
// bootstrap/stats.yaml.tmpl
// listener/client.yaml.tmpl
// listener/client_passthrough.yaml.tmpl
// listener/internal_outbound.yaml.tmpl
// listener/server.yaml.tmpl
// listener/tcp_client.yaml.tmpl
// listener/tcp_passthrough.yaml.tmpl
// listener/tcp_server.yaml.tmpl
// listener/terminate_connect.yaml.tmpl
package testdata

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"time"
)
type asset struct {
	bytes []byte
	info  os.FileInfo
}

type bindataFileInfo struct {
	name    string
	size    int64
	mode    os.FileMode
	modTime time.Time
}

// Name return file name
func (fi bindataFileInfo) Name() string {
	return fi.name
}

// Size return file size
func (fi bindataFileInfo) Size() int64 {
	return fi.size
}

// Mode return file mode
func (fi bindataFileInfo) Mode() os.FileMode {
	return fi.mode
}

// Mode return file modify time
func (fi bindataFileInfo) ModTime() time.Time {
	return fi.modTime
}

// IsDir return file whether a directory
func (fi bindataFileInfo) IsDir() bool {
	return fi.mode&os.ModeDir != 0
}

// Sys return file is sys mode
func (fi bindataFileInfo) Sys() interface{} {
	return nil
}

var _bootstrapClientYamlTmpl = []byte(`node:
  id: client
  cluster: test-cluster
  metadata: { {{ .Vars.ClientMetadata | fill }} }
admin:
  access_log_path: /dev/null
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{ .Ports.ClientAdmin }}
{{ .Vars.StatsConfig }}
dynamic_resources:
  ads_config:
    api_type: GRPC
    transport_api_version: V3
    grpc_services:
    - envoy_grpc:
        cluster_name: xds_cluster
  cds_config:
    ads: {}
    resource_api_version: V3
  lds_config:
    ads: {}
    resource_api_version: V3
static_resources:
  clusters:
  - connect_timeout: 5s
    load_assignment:
      cluster_name: xds_cluster
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{ .Ports.XDSPort }}
    http2_protocol_options: {}
    name: xds_cluster
  - name: server-outbound-cluster
    connect_timeout: 5s
    type: STATIC
    http2_protocol_options: {}
    {{- if ne .Vars.ElideServerMetadata "true" }}
    metadata:
      filter_metadata:
        istio:
          services:
            - host: server.default.svc.cluster.local
              name: server
              namespace: default
    {{- end }}
    load_assignment:
      cluster_name: server-outbound-cluster
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.2
                port_value: {{ .Ports.ServerPort }}
          {{- if eq .Vars.EnableEndpointMetadata "true" }}
          metadata:
            filter_metadata:
              istio:
                workload: ratings-v1;default;ratings;version-1;server-cluster
          {{- end }}
{{ .Vars.ClientTLSContext | indent 4 }}
{{ .Vars.ClientStaticCluster | indent 2 }}
bootstrap_extensions:
- name: envoy.bootstrap.internal_listener
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.bootstrap.internal_listener.v3.InternalListener
{{- if eq .Vars.EnableMetadataDiscovery "true" }}
- name: metadata_discovery
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/istio.workload.BootstrapExtension
    value:
      config_source:
        ads: {}
{{- end }}
`)

func bootstrapClientYamlTmplBytes() ([]byte, error) {
	return _bootstrapClientYamlTmpl, nil
}

func bootstrapClientYamlTmpl() (*asset, error) {
	bytes, err := bootstrapClientYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "bootstrap/client.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _bootstrapOtel_statsYamlTmpl = []byte(`stats_sinks:
- name: otel
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.stat_sinks.open_telemetry.v3.SinkConfig
    grpc_service:
      envoy_grpc:
        cluster_name: otel
stats_config:
  stats_matcher:
    inclusion_list:
      patterns:
      - prefix: "istiocustom."
`)

func bootstrapOtel_statsYamlTmplBytes() ([]byte, error) {
	return _bootstrapOtel_statsYamlTmpl, nil
}

func bootstrapOtel_statsYamlTmpl() (*asset, error) {
	bytes, err := bootstrapOtel_statsYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "bootstrap/otel_stats.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _bootstrapServerYamlTmpl = []byte(`node:
  id: server
  cluster: test-cluster
  metadata: { {{ .Vars.ServerMetadata | fill }} }
admin:
  access_log_path: /dev/null
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{ .Ports.ServerAdmin }}
{{ .Vars.StatsConfig }}
dynamic_resources:
  ads_config:
{{- if eq .Vars.EnableDelta "true" }}
    api_type: DELTA_GRPC
{{- else }}
    api_type: GRPC
{{- end}}
    transport_api_version: V3
    grpc_services:
    - envoy_grpc:
        cluster_name: xds_cluster
  cds_config:
    ads: {}
    resource_api_version: V3
  lds_config:
    ads: {}
    resource_api_version: V3
static_resources:
  clusters:
  - connect_timeout: 5s
    load_assignment:
      cluster_name: xds_cluster
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{ .Ports.XDSPort }}
    http2_protocol_options: {}
    name: xds_cluster
  - name: server-inbound-cluster
    connect_timeout: 5s
    type: STATIC
    {{- if eq .Vars.UsingGrpcBackend "true" }}
    http2_protocol_options: {}
    {{- end }}
    {{- if ne .Vars.ElideServerMetadata "true" }}
    metadata:
      filter_metadata:
        istio:
          services:
            - host: server.default.svc.cluster.local
              name: server
              namespace: default
    {{- end }}
    load_assignment:
      cluster_name: server-inbound-cluster
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.3
                port_value: {{ .Ports.BackendPort }}
{{ .Vars.ServerStaticCluster | indent 2 }}
{{- if ne .Vars.DisableDirectResponse "true" }}
  listeners:
  - name: staticreply
    address:
      socket_address:
        address: 127.0.0.3
        port_value: {{ .Ports.BackendPort }}
    filter_chains:
    - filters:
      - name: http
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
          stat_prefix: staticreply
          codec_type: AUTO
          route_config:
            name: staticreply
            virtual_hosts:
            - name: staticreply
              domains: ["*"]
              routes:
              - match:
                  prefix: "/"
                direct_response:
                  {{- if .Vars.DirectResponseCode }}
                  status: {{ .Vars.DirectResponseCode }}
                  {{- else }}
                  status: 200
                  {{- end }}
                  body:
                    inline_string: "hello, world!"
          http_filters:
          - name: envoy.filters.http.router
            typed_config:
              "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router
{{- end }}
bootstrap_extensions:
- name: envoy.bootstrap.internal_listener
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.bootstrap.internal_listener.v3.InternalListener
{{- if eq .Vars.EnableMetadataDiscovery "true" }}
- name: metadata_discovery
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/istio.workload.BootstrapExtension
    value:
      config_source:
        ads: {}
{{- end }}
`)

func bootstrapServerYamlTmplBytes() ([]byte, error) {
	return _bootstrapServerYamlTmpl, nil
}

func bootstrapServerYamlTmpl() (*asset, error) {
	bytes, err := bootstrapServerYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "bootstrap/server.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _bootstrapStatsYamlTmpl = []byte(`stats_config:
  use_all_default_tags: true
  stats_tags:
  - tag_name: "reporter"
    regex: "(reporter=\\.=(.*?);\\.;)"
  - tag_name: "source_namespace"
    regex: "(source_namespace=\\.=(.*?);\\.;)"
  - tag_name: "source_workload"
    regex: "(source_workload=\\.=(.*?);\\.;)"
  - tag_name: "source_canonical_service"
    regex: "(source_canonical_service=\\.=(.*?);\\.;)"
  - tag_name: "source_canonical_revision"
    regex: "(source_canonical_revision=\\.=(.*?);\\.;)"
  - tag_name: "source_workload_namespace"
    regex: "(source_workload_namespace=\\.=(.*?);\\.;)"
  - tag_name: "source_principal"
    regex: "(source_principal=\\.=(.*?);\\.;)"
  - tag_name: "source_app"
    regex: "(source_app=\\.=(.*?);\\.;)"
  - tag_name: "source_version"
    regex: "(source_version=\\.=(.*?);\\.;)"
  - tag_name: "destination_namespace"
    regex: "(destination_namespace=\\.=(.*?);\\.;)"
  - tag_name: "source_cluster"
    regex: "(source_cluster=\\.=(.*?);\\.;)"
  - tag_name: "destination_workload"
    regex: "(destination_workload=\\.=(.*?);\\.;)"
  - tag_name: "destination_workload_namespace"
    regex: "(destination_workload_namespace=\\.=(.*?);\\.;)"
  - tag_name: "destination_principal"
    regex: "(destination_principal=\\.=(.*?);\\.;)"
  - tag_name: "destination_app"
    regex: "(destination_app=\\.=(.*?);\\.;)"
  - tag_name: "destination_version"
    regex: "(destination_version=\\.=(.*?);\\.;)"
  - tag_name: "destination_service"
    regex: "(destination_service=\\.=(.*?);\\.;)"
  - tag_name: "destination_canonical_service"
    regex: "(destination_canonical_service=\\.=(.*?);\\.;)"
  - tag_name: "destination_canonical_revision"
    regex: "(destination_canonical_revision=\\.=(.*?);\\.;)"
  - tag_name: "destination_service_name"
    regex: "(destination_service_name=\\.=(.*?);\\.;)"
  - tag_name: "destination_service_namespace"
    regex: "(destination_service_namespace=\\.=(.*?);\\.;)"
  - tag_name: "destination_cluster"
    regex: "(destination_cluster=\\.=(.*?);\\.;)"
  - tag_name: "request_protocol"
    regex: "(request_protocol=\\.=(.*?);\\.;)"
  - tag_name: "response_code"
    regex: "(response_code=\\.=(.*?);\\.;)|_rq(_(\\.d{3}))$"
  - tag_name: "grpc_response_status"
    regex: "(grpc_response_status=\\.=(.*?);\\.;)"
  - tag_name: "response_flags"
    regex: "(response_flags=\\.=(.*?);\\.;)"
  - tag_name: "connection_security_policy"
    regex: "(connection_security_policy=\\.=(.*?);\\.;)"
# Extra regexes used for configurable metrics
  - tag_name: "configurable_metric_a"
    regex: "(configurable_metric_a=\\.=(.*?);\\.;)"
  - tag_name: "configurable_metric_b"
    regex: "(configurable_metric_b=\\.=(.*?);\\.;)"
  - tag_name: "route_name"
    regex: "(route_name=\\.=(.*?);\\.;)"
# Internal monitoring
  - tag_name: "cache"
    regex: "(cache\\.(.*?)\\.)"
  - tag_name: "component"
    regex: "(component\\.(.*?)\\.)"
  - tag_name: "tag"
    regex: "(tag\\.(.*?);\\.)"
  - tag_name: "wasm_filter"
    regex: "(wasm_filter\\.(.*?)\\.)"
`)

func bootstrapStatsYamlTmplBytes() ([]byte, error) {
	return _bootstrapStatsYamlTmpl, nil
}

func bootstrapStatsYamlTmpl() (*asset, error) {
	bytes, err := bootstrapStatsYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "bootstrap/stats.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerClientYamlTmpl = []byte(`{{- if ne .Vars.ClientListeners "" }}
{{ .Vars.ClientListeners }}
{{- else }}
name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Ports.ClientPort }}
filter_chains:
- filters:
  - name: http
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: client
{{ .Vars.ClientHTTPAccessLogs | fill | indent 6 }}
      http_filters:
{{ .Vars.ClientHTTPFilters | fill | indent 6 }}
      - name: envoy.filters.http.router
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router
      route_config:
        name: client
        virtual_hosts:
        - name: client
          domains: ["*"]
          routes:
          - name: client_route
            match: { prefix: / }
            route:
              {{- if .Vars.ServerClusterName }}
              cluster: {{ .Vars.ServerClusterName}}
              {{- else }}
              cluster: server-outbound-cluster
              {{- end }}
              timeout: 0s
{{- end }}
`)

func listenerClientYamlTmplBytes() ([]byte, error) {
	return _listenerClientYamlTmpl, nil
}

func listenerClientYamlTmpl() (*asset, error) {
	bytes, err := listenerClientYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/client.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerClient_passthroughYamlTmpl = []byte(`name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Ports.ClientPort }}
filter_chains:
- filters:
  - name: http
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: client
      http_filters:
      - name: connect_authority
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: type.googleapis.com/envoy.extensions.filters.http.set_filter_state.v3.Config
          value:
            on_request_headers:
            - object_key: envoy.filters.listener.original_dst.local_ip
              format_string:
                text_format_source:
                  inline_string: "%REQ(:AUTHORITY)%"
                omit_empty_values: true
              shared_with_upstream: ONCE
              skip_if_empty: true
            - object_key: envoy.filters.listener.original_dst.remote_ip
              format_string:
                text_format_source:
                  inline_string: "%DOWNSTREAM_REMOTE_ADDRESS%"
              shared_with_upstream: ONCE
      - name: envoy.filters.http.router
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router
      route_config:
        name: client
        virtual_hosts:
        - name: client
          domains: ["*"]
          routes:
          - name: client_route
            match: { prefix: / }
            route:
              cluster: tcp_passthrough
              timeout: 0s
`)

func listenerClient_passthroughYamlTmplBytes() ([]byte, error) {
	return _listenerClient_passthroughYamlTmpl, nil
}

func listenerClient_passthroughYamlTmpl() (*asset, error) {
	bytes, err := listenerClient_passthroughYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/client_passthrough.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerInternal_outboundYamlTmpl = []byte(`name: internal_outbound
internal_listener: {}
listener_filters:
- name: set_dst_address
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.filters.listener.original_dst.v3.OriginalDst
filter_chains:
- filters:
  - name: tcp_proxy
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.tcp_proxy.v3.TcpProxy
      cluster: original_dst
      tunneling_config:
        hostname: "%DOWNSTREAM_LOCAL_ADDRESS%"
        headers_to_add:
        - header:
            key: baggage
            value: k8s.deployment.name=productpage-v1
      stat_prefix: outbound
`)

func listenerInternal_outboundYamlTmplBytes() ([]byte, error) {
	return _listenerInternal_outboundYamlTmpl, nil
}

func listenerInternal_outboundYamlTmpl() (*asset, error) {
	bytes, err := listenerInternal_outboundYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/internal_outbound.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerServerYamlTmpl = []byte(`{{- if ne .Vars.ServerListeners "" }}
{{ .Vars.ServerListeners }}
{{- else }}
{{- if ne .Vars.ServerInternalAddress "" }}
name: {{ .Vars.ServerInternalAddress }}
{{- else }}
name: server
{{- end }}
traffic_direction: INBOUND
{{- if ne .Vars.ServerInternalAddress "" }}
internal_listener: {}
{{- else }}
address:
  socket_address:
    address: 127.0.0.2
    port_value: {{ .Ports.ServerPort }}
{{- end }}
filter_chains:
- filters:
{{ .Vars.ServerNetworkFilters | fill | indent 2 }}
  - name: http
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      codec_type: AUTO
      stat_prefix: server
{{ .Vars.ServerHTTPAccessLogs | fill | indent 6 }}
      http_filters:
{{ .Vars.ServerHTTPFilters | fill | indent 6 }}
      - name: envoy.filters.http.router
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router
      route_config:
        name: server
        virtual_hosts:
        - name: server
          domains: ["*"]
          routes:
          - name: server_route
            match: { prefix: / }
            route:
              cluster: server-inbound-cluster
              timeout: 0s
{{ .Vars.ServerRouteRateLimits | fill | indent 14 }}
{{ .Vars.ServerTLSContext | indent 2 }}
{{- end }}
`)

func listenerServerYamlTmplBytes() ([]byte, error) {
	return _listenerServerYamlTmpl, nil
}

func listenerServerYamlTmpl() (*asset, error) {
	bytes, err := listenerServerYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/server.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerTcp_clientYamlTmpl = []byte(`name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Ports.ClientPort }}
listener_filters:
- name: "envoy.filters.listener.tls_inspector"
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.filters.listener.tls_inspector.v3.TlsInspector
- name: "envoy.filters.listener.http_inspector"
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.filters.listener.http_inspector.v3.HttpInspector
filter_chains:
- filters:
{{ .Vars.ClientNetworkFilters | fill | indent 2 }}
  - name: tcp_proxy
    typed_config:
      "@type": type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: envoy.extensions.filters.network.tcp_proxy.v3.TcpProxy
      value:
        stat_prefix: outbound_tcp
        cluster: outbound|9080|tcp|server.default.svc.cluster.local
`)

func listenerTcp_clientYamlTmplBytes() ([]byte, error) {
	return _listenerTcp_clientYamlTmpl, nil
}

func listenerTcp_clientYamlTmpl() (*asset, error) {
	bytes, err := listenerTcp_clientYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/tcp_client.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerTcp_passthroughYamlTmpl = []byte(`name: tcp_passthrough
internal_listener: {}
listener_filters:
- name: set_dst_address
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.filters.listener.original_dst.v3.OriginalDst
filter_chains:
- filters:
  - name: connect_authority
    typed_config:
      "@type": type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: type.googleapis.com/envoy.extensions.filters.network.set_filter_state.v3.Config
      value:
        on_new_connection:
        - object_key: envoy.filters.listener.original_dst.local_ip
          format_string:
            text_format_source:
              inline_string: "%FILTER_STATE(envoy.filters.listener.original_dst.local_ip:PLAIN)%"
          shared_with_upstream: ONCE
  - name: tcp_proxy
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.tcp_proxy.v3.TcpProxy
      cluster: internal_outbound
      stat_prefix: tcp_passthrough
`)

func listenerTcp_passthroughYamlTmplBytes() ([]byte, error) {
	return _listenerTcp_passthroughYamlTmpl, nil
}

func listenerTcp_passthroughYamlTmpl() (*asset, error) {
	bytes, err := listenerTcp_passthroughYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/tcp_passthrough.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerTcp_serverYamlTmpl = []byte(`name: server
traffic_direction: INBOUND
address:
  socket_address:
    address: 127.0.0.2
    port_value: {{ .Ports.ServerPort }}
listener_filters:
- name: "envoy.filters.listener.tls_inspector"
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.filters.listener.tls_inspector.v3.TlsInspector
- name: "envoy.filters.listener.http_inspector"
  typed_config:
    "@type": type.googleapis.com/envoy.extensions.filters.listener.http_inspector.v3.HttpInspector
filter_chains:
- filters:
{{ .Vars.ServerNetworkFilters | fill | indent 2 }}
  - name: tcp_proxy
    typed_config:
      "@type": type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: envoy.extensions.filters.network.tcp_proxy.v3.TcpProxy
      value:
        stat_prefix: inbound_tcp
        cluster: inbound|9080|tcp|server.default.svc.cluster.local
{{ .Vars.ServerListenerTLSContext | indent 2 }}
`)

func listenerTcp_serverYamlTmplBytes() ([]byte, error) {
	return _listenerTcp_serverYamlTmpl, nil
}

func listenerTcp_serverYamlTmpl() (*asset, error) {
	bytes, err := listenerTcp_serverYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/tcp_server.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var _listenerTerminate_connectYamlTmpl = []byte(`name: terminate_connect
address:
  socket_address:
{{ if eq .Vars.quic "true" }}
    protocol: UDP
{{ end }}
    address: 127.0.0.2
    port_value: {{ .Ports.ServerTunnelPort }}
{{ if eq .Vars.quic "true" }}
udp_listener_config:
  quic_options: {}
  downstream_socket_config:
    prefer_gro: true
{{ end }}
filter_chains:
- filters:
  # Capture SSL info for the internal listener passthrough
  - name: envoy.filters.network.http_connection_manager
    typed_config:
      "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
      stat_prefix: terminate_connect
{{ if eq .Vars.quic "true" }}
      codec_type: HTTP3
{{ end }}
      route_config:
        name: local_route
        virtual_hosts:
        - name: local_service
          domains:
          - "*"
          routes:
          - match:
              connect_matcher:
                {}
            route:
              cluster: internal_inbound
              upgrade_configs:
              - upgrade_type: CONNECT
                connect_config:
                  {}
      http_filters:
      {{ if eq .Vars.quic "true" }}
      # TODO: accessing uriSanPeerCertificates() triggers a crash in quiche version.
      {{ else }}
      - name: authn
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: type.googleapis.com/envoy.extensions.filters.http.set_filter_state.v3.Config
          value:
            on_request_headers:
            - object_key: io.istio.peer_principal
              format_string:
                text_format_source:
                  inline_string: "%DOWNSTREAM_PEER_URI_SAN%"
              shared_with_upstream: ONCE
            - object_key: io.istio.local_principal
              format_string:
                text_format_source:
                  inline_string: "%DOWNSTREAM_LOCAL_URI_SAN%"
              shared_with_upstream: ONCE
      {{ end }}
      - name: peer_metadata
        typed_config:
          "@type": type.googleapis.com/udpa.type.v1.TypedStruct
          type_url: type.googleapis.com/io.istio.http.peer_metadata.Config
          value:
            downstream_discovery:
            - baggage: {}
            shared_with_upstream: true
      - name: envoy.filters.http.router
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router
      http2_protocol_options:
        allow_connect: true
      upgrade_configs:
      - upgrade_type: CONNECT
  transport_socket:
{{ if eq .Vars.quic "true" }}
    name: quic
    typed_config:
      "@type": type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: type.googleapis.com/envoy.extensions.transport_sockets.quic.v3.QuicDownstreamTransport
      value:
        downstream_tls_context:
          common_tls_context:
            tls_certificate_sds_secret_configs:
              name: server
              sds_config:
                api_config_source:
                  api_type: GRPC
                  grpc_services:
                  - envoy_grpc:
                      cluster_name: xds_cluster
                  set_node_on_first_message_only: true
                  transport_api_version: V3
                resource_api_version: V3
            validation_context:
              trusted_ca: { filename: "testdata/certs/root.cert" }
#         require_client_certificate: true # XXX: This setting is ignored ATM per @danzh.
{{ else }}
    name: tls
    typed_config:
      "@type": type.googleapis.com/udpa.type.v1.TypedStruct
      type_url: envoy.extensions.transport_sockets.tls.v3.DownstreamTlsContext
      value:
        common_tls_context:
          tls_certificate_sds_secret_configs:
            name: server
            sds_config:
              api_config_source:
                api_type: GRPC
                grpc_services:
                - envoy_grpc:
                    cluster_name: xds_cluster
                set_node_on_first_message_only: true
                transport_api_version: V3
              resource_api_version: V3
          validation_context:
            trusted_ca: { filename: "testdata/certs/root.cert" }
        require_client_certificate: true
{{ end }}
`)

func listenerTerminate_connectYamlTmplBytes() ([]byte, error) {
	return _listenerTerminate_connectYamlTmpl, nil
}

func listenerTerminate_connectYamlTmpl() (*asset, error) {
	bytes, err := listenerTerminate_connectYamlTmplBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "listener/terminate_connect.yaml.tmpl", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

// Asset loads and returns the asset for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func Asset(name string) ([]byte, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		a, err := f()
		if err != nil {
			return nil, fmt.Errorf("Asset %s can't read by error: %v", name, err)
		}
		return a.bytes, nil
	}
	return nil, fmt.Errorf("Asset %s not found", name)
}

// MustAsset is like Asset but panics when Asset would return an error.
// It simplifies safe initialization of global variables.
func MustAsset(name string) []byte {
	a, err := Asset(name)
	if err != nil {
		panic("asset: Asset(" + name + "): " + err.Error())
	}

	return a
}

// AssetInfo loads and returns the asset info for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func AssetInfo(name string) (os.FileInfo, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		a, err := f()
		if err != nil {
			return nil, fmt.Errorf("AssetInfo %s can't read by error: %v", name, err)
		}
		return a.info, nil
	}
	return nil, fmt.Errorf("AssetInfo %s not found", name)
}

// AssetNames returns the names of the assets.
func AssetNames() []string {
	names := make([]string, 0, len(_bindata))
	for name := range _bindata {
		names = append(names, name)
	}
	return names
}

// _bindata is a table, holding each asset generator, mapped to its name.
var _bindata = map[string]func() (*asset, error){
	"bootstrap/client.yaml.tmpl":            bootstrapClientYamlTmpl,
	"bootstrap/otel_stats.yaml.tmpl":        bootstrapOtel_statsYamlTmpl,
	"bootstrap/server.yaml.tmpl":            bootstrapServerYamlTmpl,
	"bootstrap/stats.yaml.tmpl":             bootstrapStatsYamlTmpl,
	"listener/client.yaml.tmpl":             listenerClientYamlTmpl,
	"listener/client_passthrough.yaml.tmpl": listenerClient_passthroughYamlTmpl,
	"listener/internal_outbound.yaml.tmpl":  listenerInternal_outboundYamlTmpl,
	"listener/server.yaml.tmpl":             listenerServerYamlTmpl,
	"listener/tcp_client.yaml.tmpl":         listenerTcp_clientYamlTmpl,
	"listener/tcp_passthrough.yaml.tmpl":    listenerTcp_passthroughYamlTmpl,
	"listener/tcp_server.yaml.tmpl":         listenerTcp_serverYamlTmpl,
	"listener/terminate_connect.yaml.tmpl":  listenerTerminate_connectYamlTmpl,
}

// AssetDir returns the file names below a certain
// directory embedded in the file by go-bindata.
// For example if you run go-bindata on data/... and data contains the
// following hierarchy:
//     data/
//       foo.txt
//       img/
//         a.png
//         b.png
// then AssetDir("data") would return []string{"foo.txt", "img"}
// AssetDir("data/img") would return []string{"a.png", "b.png"}
// AssetDir("foo.txt") and AssetDir("notexist") would return an error
// AssetDir("") will return []string{"data"}.
func AssetDir(name string) ([]string, error) {
	node := _bintree
	if len(name) != 0 {
		cannonicalName := strings.Replace(name, "\\", "/", -1)
		pathList := strings.Split(cannonicalName, "/")
		for _, p := range pathList {
			node = node.Children[p]
			if node == nil {
				return nil, fmt.Errorf("Asset %s not found", name)
			}
		}
	}
	if node.Func != nil {
		return nil, fmt.Errorf("Asset %s not found", name)
	}
	rv := make([]string, 0, len(node.Children))
	for childName := range node.Children {
		rv = append(rv, childName)
	}
	return rv, nil
}

type bintree struct {
	Func     func() (*asset, error)
	Children map[string]*bintree
}

var _bintree = &bintree{nil, map[string]*bintree{
	"bootstrap": &bintree{nil, map[string]*bintree{
		"client.yaml.tmpl":     &bintree{bootstrapClientYamlTmpl, map[string]*bintree{}},
		"otel_stats.yaml.tmpl": &bintree{bootstrapOtel_statsYamlTmpl, map[string]*bintree{}},
		"server.yaml.tmpl":     &bintree{bootstrapServerYamlTmpl, map[string]*bintree{}},
		"stats.yaml.tmpl":      &bintree{bootstrapStatsYamlTmpl, map[string]*bintree{}},
	}},
	"listener": &bintree{nil, map[string]*bintree{
		"client.yaml.tmpl":             &bintree{listenerClientYamlTmpl, map[string]*bintree{}},
		"client_passthrough.yaml.tmpl": &bintree{listenerClient_passthroughYamlTmpl, map[string]*bintree{}},
		"internal_outbound.yaml.tmpl":  &bintree{listenerInternal_outboundYamlTmpl, map[string]*bintree{}},
		"server.yaml.tmpl":             &bintree{listenerServerYamlTmpl, map[string]*bintree{}},
		"tcp_client.yaml.tmpl":         &bintree{listenerTcp_clientYamlTmpl, map[string]*bintree{}},
		"tcp_passthrough.yaml.tmpl":    &bintree{listenerTcp_passthroughYamlTmpl, map[string]*bintree{}},
		"tcp_server.yaml.tmpl":         &bintree{listenerTcp_serverYamlTmpl, map[string]*bintree{}},
		"terminate_connect.yaml.tmpl":  &bintree{listenerTerminate_connectYamlTmpl, map[string]*bintree{}},
	}},
}}

// RestoreAsset restores an asset under the given directory
func RestoreAsset(dir, name string) error {
	data, err := Asset(name)
	if err != nil {
		return err
	}
	info, err := AssetInfo(name)
	if err != nil {
		return err
	}
	err = os.MkdirAll(_filePath(dir, filepath.Dir(name)), os.FileMode(0755))
	if err != nil {
		return err
	}
	err = ioutil.WriteFile(_filePath(dir, name), data, info.Mode())
	if err != nil {
		return err
	}
	err = os.Chtimes(_filePath(dir, name), info.ModTime(), info.ModTime())
	if err != nil {
		return err
	}
	return nil
}

// RestoreAssets restores an asset under the given directory recursively
func RestoreAssets(dir, name string) error {
	children, err := AssetDir(name)
	// File
	if err != nil {
		return RestoreAsset(dir, name)
	}
	// Dir
	for _, child := range children {
		err = RestoreAssets(dir, filepath.Join(name, child))
		if err != nil {
			return err
		}
	}
	return nil
}

func _filePath(dir, name string) string {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	return filepath.Join(append([]string{dir}, strings.Split(cannonicalName, "/")...)...)
}
