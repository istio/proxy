module istio.io/proxy

go 1.12

replace cloud.google.com/go/meshtelemetry/v1alpha1 v0.0.0 => ./test/envoye2e/stackdriver_plugin/edges

require (
	cloud.google.com/go/meshtelemetry/v1alpha1 v0.0.0
	github.com/bianpengyuan/istio-wasm-sdk v0.0.0-20200412041507-d2b3614714ae
	github.com/cncf/udpa/go v0.0.0-20200327203949-e8cd3a4bb307
	github.com/d4l3k/messagediff v1.2.2-0.20180726183240-b9e99b2f9263
	github.com/envoyproxy/go-control-plane v0.9.5
	github.com/ghodss/yaml v1.0.0
	github.com/gogo/status v1.1.0
	github.com/golang/protobuf v1.3.3
	github.com/prometheus/client_model v0.2.0
	github.com/prometheus/common v0.9.1
	google.golang.org/api v0.20.0
	google.golang.org/genproto v0.0.0-20190819201941-24fa4b261c55
	google.golang.org/grpc v1.27.1
	istio.io/api v0.0.0-20200222035036-b245c555a47b // indirect
	istio.io/gogo-genproto v0.0.0-20200222040034-75d4aa95f22c // indirect
)
