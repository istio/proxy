module istio.io/proxy

go 1.24.0

require (
	github.com/cncf/xds/go v0.0.0-20251110193048-8bfbf64dc13e
	github.com/envoyproxy/go-control-plane v0.14.1-0.20251120180717-7c66c7f1d0b2
	github.com/envoyproxy/go-control-plane/envoy v1.36.0
	github.com/golang/protobuf v1.5.4
	github.com/google/go-cmp v0.7.0
	github.com/prometheus/client_model v0.6.2
	github.com/prometheus/common v0.46.0
	go.opentelemetry.io/proto/otlp v1.7.1
	go.starlark.net v0.0.0-20240123142251-f86470692795
	google.golang.org/genproto/googleapis/rpc v0.0.0-20250825161204-c5933d9347a5
	google.golang.org/grpc v1.76.0
	google.golang.org/protobuf v1.36.10
	gopkg.in/yaml.v2 v2.4.0
	sigs.k8s.io/yaml v1.4.0
)

require (
	cel.dev/expr v0.24.0 // indirect
	github.com/envoyproxy/go-control-plane/ratelimit v0.1.0 // indirect
	github.com/envoyproxy/protoc-gen-validate v1.2.1 // indirect
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.27.1 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/planetscale/vtprotobuf v0.6.1-0.20240319094008-0393e58bdf10 // indirect
	golang.org/x/net v0.43.0 // indirect
	golang.org/x/sys v0.35.0 // indirect
	golang.org/x/text v0.28.0 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20250825161204-c5933d9347a5 // indirect
)
