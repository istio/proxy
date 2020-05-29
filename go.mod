module istio.io/proxy

go 1.12

replace cloud.google.com/go/meshtelemetry/v1alpha1 v0.0.0 => ./test/envoye2e/stackdriver_plugin/edges

require (
	cloud.google.com/go/meshtelemetry/v1alpha1 v0.0.0
	github.com/bazelbuild/rules_go v0.23.0
	github.com/cncf/udpa/go v0.0.0-20200327203949-e8cd3a4bb307
	github.com/d4l3k/messagediff v1.2.2-0.20180726183240-b9e99b2f9263
	github.com/envoyproxy/go-control-plane v0.9.5
	github.com/ghodss/yaml v1.0.0
	github.com/gogo/protobuf v1.1.1
	github.com/golang/protobuf v1.3.5
	github.com/google/flatbuffers v1.12.0
	github.com/google/pprof v0.0.0-20200507031123-427632fa3b1c // indirect
	github.com/googleapis/gax-go v2.0.2+incompatible // indirect
	github.com/lightstep/lightstep-tracer-cpp v0.12.0
	github.com/pkg/sftp v1.11.0 // indirect
	github.com/prometheus/client_model v0.2.0
	github.com/prometheus/common v0.9.1
	github.com/spf13/afero v1.2.2 // indirect
	go4.org v0.0.0-20200411211856-f5505b9728dd // indirect
	golang.org/x/arch v0.0.0-20200511175325-f7c78586839d // indirect
	golang.org/x/build v0.0.0-20200514024326-6c8dc32a9801 // indirect
	golang.org/x/net v0.0.0-20200324143707-d3edc9973b7e
	golang.org/x/sys v0.0.0-20200331124033-c3d80250170d
	golang.org/x/text v0.3.2
	golang.org/x/tools v0.0.0-20200515010526-7d3b6ebf133d
	google.golang.org/api v0.24.0 // indirect
	google.golang.org/genproto v0.0.0-20200331122359-1ee6d9798940
	google.golang.org/grpc v1.28.0
	gopkg.in/yaml.v2 v2.2.8 // indirect
)
