module istio.io/proxy

go 1.24

require (
	github.com/cncf/xds/go v0.0.0-20241223141626-cff3c89139a3
	github.com/envoyproxy/go-control-plane v0.13.5-0.20250405152605-3d52a2c138ee
	github.com/envoyproxy/go-control-plane/envoy v1.32.4
	github.com/golang/protobuf v1.5.4
	github.com/google/go-cmp v0.7.0
	github.com/prometheus/client_model v0.6.1
	github.com/prometheus/common v0.46.0
	go.opentelemetry.io/proto/otlp v1.1.0
	go.starlark.net v0.0.0-20240123142251-f86470692795
	google.golang.org/genproto/googleapis/rpc v0.0.0-20250115164207-1a7da9e5054f
	google.golang.org/grpc v1.71.0
	google.golang.org/protobuf v1.36.6
	gopkg.in/yaml.v2 v2.4.0
	sigs.k8s.io/yaml v1.4.0
)

require (
	cel.dev/expr v0.19.1 // indirect
	github.com/envoyproxy/go-control-plane/ratelimit v0.1.0 // indirect
	github.com/envoyproxy/protoc-gen-validate v1.2.1 // indirect
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.19.0 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/planetscale/vtprotobuf v0.6.1-0.20240319094008-0393e58bdf10 // indirect
	golang.org/x/net v0.36.0 // indirect
	golang.org/x/sys v0.30.0 // indirect
	golang.org/x/text v0.22.0 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20250106144421-5f5ef82da422 // indirect
)
