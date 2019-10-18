package driver

import (
	"strconv"
	"testing"
	"time"
)

const SDClientMetadata = `
"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "productpage",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-productpage",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://api/apps/v1/namespaces/default/deployment/productpage-v1",
"WORKLOAD_NAME": "productpage-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container productpage",
"POD_NAME": "productpage-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_gke_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_location": "us-east4-b"
},
"LABELS": {
 "app": "productpage",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "productpage-v1-84975bc778-pxz2w"`

const SDServerMetadata = `
"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "ratings",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-ratings",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://api/apps/v1/namespaces/default/deployment/ratings-v1",
"WORKLOAD_NAME": "ratings-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container ratings",
"POD_NAME": "ratings-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_gke_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_location": "us-east4-b"
},
"LABELS": {
 "app": "ratings",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "ratings-v1-84975bc778-pxz2w"`

const StackdriverClientHTTPListener = `
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
      - name: envoy.filters.http.wasm
        config:
          config:
            vm_config:
              runtime: "envoy.wasm.runtime.null"
              code:
                inline_string: "envoy.wasm.metadata_exchange"
            configuration: "test"
      - name: envoy.filters.http.wasm
        config:
          config:
            root_id: "stackdriver_outbound"
            vm_config:
              vm_id: "stackdriver_outbound"
              runtime: "envoy.wasm.runtime.null"
              code:
                inline_string: "envoy.wasm.null.stackdriver"
            configuration: >-
              {
                "testMonitoringEndpoint": "127.0.0.1:{{ .Vars.SDPort }}",
                "testLoggingEndpoint": "127.0.0.1:{{ .Vars.SDPort }}",
              }
      - name: envoy.router
      route_config:
        name: client
        virtual_hosts:
        - name: client
          domains: ["*"]
          routes:
          - match: { prefix: / }
            route:
              cluster: server
              timeout: 0s
`

const StackdriverServerHTTPListener = `
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
      - name: envoy.filters.http.wasm
        config:
          config:
            vm_config:
              runtime: "envoy.wasm.runtime.null"
              code:
                inline_string: "envoy.wasm.metadata_exchange"
            configuration: "test"
      - name: envoy.filters.http.wasm
        config:
          config:
            root_id: "stackdriver_inbound"
            vm_config:
              vm_id: "stackdriver_inbound"
              runtime: "envoy.wasm.runtime.null"
              code:
                inline_string: "envoy.wasm.null.stackdriver"
            configuration: >-
              {
                "testMonitoringEndpoint": "127.0.0.1:{{ .Vars.SDPort }}",
                "testLoggingEndpoint": "127.0.0.1:{{ .Vars.SDPort }}",
              }
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

func TestStackDriver(t *testing.T) {
	ports := Counter(19010)
	sd := &Stackdriver{}
	if err := (&Scenario{
		[]Step{
			&XDS{},
			sd,
			&Update{Node: "client", Version: "0", Listeners: []string{StackdriverClientHTTPListener}},
			&Update{Node: "server", Version: "0", Listeners: []string{StackdriverServerHTTPListener}},
			&Envoy{Bootstrap: ServerBootstrap},
			&Envoy{Bootstrap: ClientBootstrap},
			&Sleep{1 * time.Second},
			&Get{19011, "hello, world!"},
		},
	}).Run(&Params{
		Vars: map[string]string{
			"BackendPort":    strconv.Itoa(ports()),
			"ClientPort":     strconv.Itoa(ports()),
			"SDPort":         strconv.Itoa(ports()),
			"ClientAdmin":    strconv.Itoa(ports()),
			"ServerAdmin":    strconv.Itoa(ports()),
			"ServerPort":     strconv.Itoa(ports()),
			"ClientMetadata": SDClientMetadata,
			"ServerMetadata": SDServerMetadata,
		},
		XDS: ports(),
	}); err != nil {
		t.Fatal(err)
	}
}
