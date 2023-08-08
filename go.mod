module istio.io/proxy

go 1.19

require (
	cloud.google.com/go/logging v1.7.0
	cloud.google.com/go/monitoring v1.13.0
	cloud.google.com/go/trace v1.9.0
	github.com/cncf/xds/go v0.0.0-20230607035331-e9ce68804cb4
	github.com/envoyproxy/go-control-plane v0.11.2-0.20230725211550-11bfe846bcd4
	github.com/golang/protobuf v1.5.3
	github.com/google/go-cmp v0.5.9
	github.com/prometheus/client_model v0.4.0
	github.com/prometheus/common v0.38.0
	go.opentelemetry.io/proto/otlp v1.0.0
	google.golang.org/genproto/googleapis/api v0.0.0-20230530153820-e85fd2cbaebc
	google.golang.org/genproto/googleapis/rpc v0.0.0-20230530153820-e85fd2cbaebc
	google.golang.org/grpc v1.56.2
	google.golang.org/protobuf v1.31.0
	gopkg.in/yaml.v2 v2.4.0
	sigs.k8s.io/yaml v1.3.0
)

require (
	cloud.google.com/go/longrunning v0.4.1 // indirect
	github.com/census-instrumentation/opencensus-proto v0.4.1 // indirect
	github.com/envoyproxy/protoc-gen-validate v1.0.1 // indirect
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.16.0 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.2 // indirect
	golang.org/x/net v0.10.0 // indirect
	golang.org/x/sys v0.8.0 // indirect
	golang.org/x/text v0.9.0 // indirect
	google.golang.org/genproto v0.0.0-20230526203410-71b5a4ffd15e // indirect
)
