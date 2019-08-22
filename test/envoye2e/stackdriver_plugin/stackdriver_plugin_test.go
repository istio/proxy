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
        "monitoringEndpoint": "localhost:12312",
      }`

const outboundNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "productpage",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-productpage",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://api/apps/v1/namespaces/default/deployment/productpage-v1",
"WORKLOAD_NAME": "productpage-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container productpage",
"POD_NAME": "productpage-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "productpage",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "productpage-v1-84975bc778-pxz2w",`

const inboundNodeMetadata = `"NAMESPACE": "default",
"INCLUDE_INBOUND_PORTS": "9080",
"app": "ratings",
"EXCHANGE_KEYS": "NAME,NAMESPACE,INSTANCE_IPS,LABELS,OWNER,PLATFORM_METADATA,WORKLOAD_NAME,CANONICAL_TELEMETRY_SERVICE,MESH_ID,SERVICE_ACCOUNT",
"INSTANCE_IPS": "10.52.0.34,fe80::a075:11ff:fe5e:f1cd",
"pod-template-hash": "84975bc778",
"INTERCEPTION_MODE": "REDIRECT",
"SERVICE_ACCOUNT": "bookinfo-ratings",
"CONFIG_NAMESPACE": "default",
"version": "v1",
"OWNER": "kubernetes://api/apps/v1/namespaces/default/deployment/ratings-v1",
"WORKLOAD_NAME": "ratings-v1",
"ISTIO_VERSION": "1.3-dev",
"kubernetes.io/limit-ranger": "LimitRanger plugin set: cpu request for container ratings",
"POD_NAME": "ratings-v1-84975bc778-pxz2w",
"istio": "sidecar",
"PLATFORM_METADATA": {
 "gcp_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_cluster_location": "us-east4-b"
},
"LABELS": {
 "app": "ratings",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "ratings-v1-84975bc778-pxz2w",`

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
