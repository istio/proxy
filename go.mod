module istio.io/proxy

go 1.19

require (
	cloud.google.com/go/logging v1.7.0
	cloud.google.com/go/monitoring v1.12.0
	cloud.google.com/go/trace v1.8.0
	github.com/cncf/xds/go v0.0.0-20230310173818-32f1caf87195
	github.com/envoyproxy/go-control-plane v0.11.1-0.20230526184532-dd0b405b25bf
	github.com/golang/protobuf v1.5.3
	github.com/google/go-cmp v0.5.9
	github.com/prometheus/client_model v0.4.0
	github.com/prometheus/common v0.38.0
	google.golang.org/genproto v0.0.0-20230306155012-7f2fa6fef1f4
	google.golang.org/grpc v1.55.0
	google.golang.org/protobuf v1.30.0
	gopkg.in/yaml.v2 v2.4.0
	sigs.k8s.io/yaml v1.3.0
)

require (
	cloud.google.com/go/longrunning v0.4.1 // indirect
	github.com/census-instrumentation/opencensus-proto v0.4.1 // indirect
	github.com/envoyproxy/protoc-gen-validate v0.10.0 // indirect
	github.com/kr/pretty v0.3.1 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.2 // indirect
	golang.org/x/net v0.8.0 // indirect
	golang.org/x/sys v0.6.0 // indirect
	golang.org/x/text v0.8.0 // indirect
)
