package driver

import (
	"testing"
	"time"
)

const ClientBootstrap = `
node:
  id: client
  cluster: test-cluster
admin:
  access_log_path: /dev/null
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{ .Vars.ClientAdmin }}
dynamic_resources:
  ads_config:
    api_type: GRPC
    grpc_services:
    - envoy_grpc:
        cluster_name: xds_cluster
  cds_config:
    ads: {}
  lds_config:
    ads: {}
static_resources:
  clusters:
  - connect_timeout: 1s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: {{ .XDS }}
    http2_protocol_options: {}
    name: xds_cluster
  - name: server
    connect_timeout: 1s
    type: STATIC
    load_assignment:
      cluster_name: server
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{ .Vars.ServerPort }}
`

const ServerBootstrap = `
node:
  id: server
  cluster: test-cluster
admin:
  access_log_path: /dev/null
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{ .Vars.ServerAdmin }}
dynamic_resources:
  ads_config:
    api_type: GRPC
    grpc_services:
    - envoy_grpc:
        cluster_name: xds_cluster
  cds_config:
    ads: {}
  lds_config:
    ads: {}
static_resources:
  clusters:
  - connect_timeout: 1s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: {{ .XDS }}
    http2_protocol_options: {}
    name: xds_cluster
  - name: backend
    connect_timeout: 1s
    type: STATIC
    load_assignment:
      cluster_name: backend
      endpoints:
      - lb_endpoints:
        - endpoint:
            address:
              socket_address:
                address: 127.0.0.1
                port_value: {{ .Vars.BackendPort }}
`

const ClientHTTPListener = `
name: client
traffic_direction: OUTBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Vars.ClientPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    config:
      codec_type: AUTO
      stat_prefix: client
      http_filters:
      - name: envoy.router
      route_config:
        name: client
        virtual_hosts:
        - name: client
          domains: ["*"]
          routes:
          - match: { prefix: / }
            route:
              cluster: client
              timeout: 0s
`

const ServerHTTPListener = `
name: server
traffic_direction: INBOUND
address:
  socket_address:
    address: 127.0.0.1
    port_value: {{ .Vars.ServerPort }}
filter_chains:
- filters:
  - name: envoy.http_connection_manager
    config:
      codec_type: AUTO
      stat_prefix: server
      http_filters:
      - name: envoy.router
      route_config:
        name: server
        virtual_hosts:
        - name: server
          domains: ["*"]
          routes:
          - match: { prefix: / }
            route:
              cluster: backend
              timeout: 0s
`

func counter(base int) func() int {
	state := base - 1
	return func() int {
		state++
		return state
	}
}

func TestBasicHTTP(t *testing.T) {
	ports := counter(19000)
	if err := (&Scenario{
		[]Step{
			&XDS{},
			&Backend{Port: 19000},
			&Envoy{Bootstrap: ServerBootstrap},
			&Envoy{Bootstrap: ClientBootstrap},
			&Update{Node: "client", Version: "0", Listeners: []string{ClientHTTPListener}},
			&Update{Node: "server", Version: "0", Listeners: []string{ServerHTTPListener}},
			&Sleep{1 * time.Second},
			&Get{19001},
		},
	}).Run(&Params{
		Vars: map[string]interface{}{
			"BackendPort": ports(),
			"ClientPort":  ports(),
			"ClientAdmin": ports(),
			"ServerAdmin": ports(),
			"ServerPort":  ports(),
		},
		XDS: ports(),
	}); err != nil {
		t.Fatal(err)
	}
}
