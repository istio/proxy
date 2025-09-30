module istio.io/proxy

go 1.24.0

require (
	github.com/cncf/xds/go v0.0.0-20250501225837-2ac532fd4443
	github.com/envoyproxy/go-control-plane v0.13.5-0.20250816044120-872f08a0dcd7
	github.com/envoyproxy/go-control-plane/envoy v1.32.5-0.20250902220953-4a410393a630
	github.com/golang/protobuf v1.5.4
	github.com/google/go-cmp v0.7.0
	github.com/prometheus/client_model v0.6.2
	github.com/prometheus/common v0.46.0
	go.opentelemetry.io/proto/otlp v1.7.1
	go.starlark.net v0.0.0-20240123142251-f86470692795
	google.golang.org/genproto/googleapis/rpc v0.0.0-20250728155136-f173205681a0
	google.golang.org/grpc v1.74.2
	google.golang.org/protobuf v1.36.7
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
	golang.org/x/net v0.42.0 // indirect
	golang.org/x/sys v0.34.0 // indirect
	golang.org/x/text v0.27.0 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20250728155136-f173205681a0 // indirect
)
