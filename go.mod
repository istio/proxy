module istio.io/proxy

go 1.19

require (
	cloud.google.com/go/logging v1.8.1
	cloud.google.com/go/monitoring v1.16.0
	cloud.google.com/go/trace v1.10.1
	github.com/cncf/xds/go v0.0.0-20230607035331-e9ce68804cb4
	github.com/envoyproxy/go-control-plane v0.11.2-0.20231026140209-dc05a22efe95
	github.com/golang/protobuf v1.5.3
	github.com/google/go-cmp v0.5.9
	github.com/prometheus/client_model v0.5.0
	github.com/prometheus/common v0.44.0
	go.opentelemetry.io/proto/otlp v1.0.0
	go.starlark.net v0.0.0-20231016134836-22325403fcb3
	google.golang.org/genproto/googleapis/api v0.0.0-20230920204549-e6e6cdab5c13
	google.golang.org/genproto/googleapis/rpc v0.0.0-20231009173412-8bfb1ae86b6c
	google.golang.org/grpc v1.58.3
	google.golang.org/protobuf v1.31.0
	gopkg.in/yaml.v2 v2.4.0
	sigs.k8s.io/yaml v1.3.0
)

require (
	cloud.google.com/go/longrunning v0.5.1 // indirect
	github.com/census-instrumentation/opencensus-proto v0.4.1 // indirect
	github.com/envoyproxy/protoc-gen-validate v1.0.2 // indirect
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.16.0 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.4 // indirect
	golang.org/x/net v0.17.0 // indirect
	golang.org/x/sys v0.13.0 // indirect
	golang.org/x/text v0.13.0 // indirect
	google.golang.org/genproto v0.0.0-20231002182017-d307bd883b97 // indirect
)
