// Copyright 2019 Istio Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package client_test

import (
	"fmt"
	"testing"
	"time"

	"github.com/golang/protobuf/jsonpb"
	"istio.io/proxy/test/envoye2e/env"
	fs "istio.io/proxy/test/envoye2e/stackdriver_plugin/fake_stackdriver"

	"github.com/golang/protobuf/proto"
	monitoringpb "google.golang.org/genproto/googleapis/monitoring/v3"
)

const outboundStackdriverFilter = `- name: envoy.wasm
  config:
    vm_config:
      vm: "envoy.wasm.vm.null"
      code:
        inline_string: "envoy.wasm.metadata_exchange"
    configuration: "test"
- name: envoy.wasm
  config:
    vm_config:
      vm: "envoy.wasm.vm.null"
      code:
        inline_string: "envoy.wasm.null.stackdriver"
    configuration: >-
      {
        "kind": "OUTBOUND",
        "monitoringEndpoint": "localhost:12312",
      }`

const inboundStackdriverFilter = `- name: envoy.wasm
  config:
    vm_config:
      vm: "envoy.wasm.vm.null"
      code:
        inline_string: "envoy.wasm.metadata_exchange"
    configuration: "test"
- name: envoy.wasm
  config:
    vm_config:
      vm: "envoy.wasm.vm.null"
      code:
        inline_string: "envoy.wasm.null.stackdriver"
    configuration: >-
      {
        "kind": "INBOUND",
        "monitoringEndpoint": "localhost:12312",
      }`

const outboundNodeMetadata = `istio.io/metadata: 
  labels: 
    app: client
    version: v1
  name: client-pod
  namespace: client-namespace
  platform_metadata:
    gcp_project: test-project
    gcp_cluster_location: test-location
    gcp_cluster_name: test-cluster
  owner: "kubernetes://apis/v1/namespaces/client-namespace/pod/client"
  ports_to_containers: 
    80: client-container
  workload_name: client`

const inboundNodeMetadata = `istio.io/metadata: 
  labels: 
    app: server
    version: v1
  name: server-pod
  namespace: server-namespace
  platform_metadata:
    gcp_project: test-project
    gcp_cluster_location: test-location
    gcp_cluster_name: test-cluster
  owner: "kubernetes://apis/v1/namespaces/server-namespace/pod/server"
  ports_to_containers: 
    80: server-container
  workload_name: server`

func compareTimeSeries(got, want *monitoringpb.TimeSeries) error {
	// ignore time difference
	got.Points[0].Interval = nil
	// remove opencensus_task label.
	// TODO: remove this after https://github.com/census-instrumentation/opencensus-cpp/issues/372
	delete(got.Metric.Labels, "opencensus_task")
	if !proto.Equal(want, got) {
		return fmt.Errorf("request count timeseries is not expected, got %v \nwant %v\n", got, want)
	}
	return nil
}

func verifyCreateTimeSeriesReq(got *monitoringpb.CreateTimeSeriesRequest) error {
	var srvReqCount, cltReqCount monitoringpb.TimeSeries
	jsonpb.UnmarshalString(fs.ServerRequestCountJSON, &srvReqCount)
	jsonpb.UnmarshalString(fs.ClientRequestCountJSON, &cltReqCount)
	for _, t := range got.TimeSeries {
		if t.Metric.Type == srvReqCount.Metric.Type {
			return compareTimeSeries(t, &srvReqCount)
		}
		if t.Metric.Type == cltReqCount.Metric.Type {
			return compareTimeSeries(t, &cltReqCount)
		}
	}
	// at least one time series should match either client side request count or server side request count.
	return fmt.Errorf("cannot find expected request count from creat time series request %v", got)
}

func TestStackdriverPlugin(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.StackdriverPluginTest, t)
	fsd := fs.NewFakeStackdriver(12312)
	s.SetFiltersBeforeEnvoyRouterInClientToProxy(outboundStackdriverFilter)
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(inboundStackdriverFilter)
	s.SetServerNodeMetadata(inboundNodeMetadata)
	s.SetClientNodeMetadata(outboundNodeMetadata)
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	url := fmt.Sprintf("http://localhost:%d/echo", s.Ports().AppToClientProxyPort)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	for i := 0; i < 10; i++ {
		if _, _, err := env.HTTPGet(url); err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
	}

	for i := 0; i < 2; i++ {
		// Two requests should be recevied: one from client and one from server.
		select {
		case req := <-fsd.RcvReq:
			if err := verifyCreateTimeSeriesReq(req); err != nil {
				t.Errorf("CreateTimeSeries verification failed: %v", err)
			}
		case <-time.After(20 * time.Second):
			t.Error("timeout on waiting Stackdriver server to receive request")
		}
	}
}
