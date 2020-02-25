module istio.io/proxy

go 1.12

replace cloud.google.com/go/meshtelemetry/v1alpha1 v0.0.0 => ./test/envoye2e/stackdriver_plugin/edges

require (
	cloud.google.com/go/meshtelemetry/v1alpha1 v0.0.0
	github.com/cncf/udpa/go v0.0.0-20191209042840-269d4d468f6f
	github.com/d4l3k/messagediff v1.2.2-0.20180726183240-b9e99b2f9263
	github.com/envoyproxy/go-control-plane v0.9.3
	github.com/ghodss/yaml v1.0.0
	github.com/gogo/protobuf v1.3.1 // indirect
	github.com/golang/protobuf v1.3.2
	github.com/prometheus/client_model v0.0.0-20190812154241-14fe0d1b01d4
	github.com/prometheus/common v0.7.0
	google.golang.org/genproto v0.0.0-20190819201941-24fa4b261c55
	google.golang.org/grpc v1.25.1
	gopkg.in/yaml.v2 v2.2.4 // indirect
	istio.io/api v0.0.0-20200222035036-b245c555a47b // indirect
	istio.io/gogo-genproto v0.0.0-20200222040034-75d4aa95f22c // indirect
)
