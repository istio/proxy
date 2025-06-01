module istio.io/proxy

go 1.24

require (
	github.com/cncf/xds/go v0.0.0-20250121191232-2f005788dc42
	github.com/envoyproxy/go-control-plane v0.13.5-0.20250529131204-17588c61bef7
	github.com/envoyproxy/go-control-plane/envoy v1.32.4
	github.com/golang/protobuf v1.5.4
	github.com/google/go-cmp v0.7.0
	github.com/prometheus/client_model v0.6.1
	github.com/prometheus/common v0.46.0
	go.opentelemetry.io/proto/otlp v1.1.0
	go.starlark.net v0.0.0-20240123142251-f86470692795
	google.golang.org/genproto/googleapis/rpc v0.0.0-20250428153025-10db94c68c34
	google.golang.org/grpc v1.72.2
	google.golang.org/protobuf v1.36.6
	gopkg.in/yaml.v2 v2.4.0
	sigs.k8s.io/yaml v1.4.0
)

require (
	cel.dev/expr v0.20.0 // indirect
	github.com/envoyproxy/go-control-plane/ratelimit v0.1.0 // indirect
	github.com/envoyproxy/protoc-gen-validate v1.2.1 // indirect
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.19.0 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/planetscale/vtprotobuf v0.6.1-0.20240319094008-0393e58bdf10 // indirect
	golang.org/x/net v0.39.0 // indirect
	golang.org/x/sys v0.32.0 // indirect
	golang.org/x/text v0.24.0 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20250428153025-10db94c68c34 // indirect
)
