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

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
	fs "istio.io/proxy/test/envoye2e/stackdriver_plugin/fake_stackdriver"

	"github.com/golang/protobuf/proto"
	logging "google.golang.org/genproto/googleapis/logging/v2"
	monitoringpb "google.golang.org/genproto/googleapis/monitoring/v3"
)

const outboundStackdriverFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          inline_string: "envoy.wasm.metadata_exchange"
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stackdriver_outbound"
      vm_config:
        vm_id: "stackdriver_outbound"
        runtime: "envoy.wasm.runtime.null"
        code:
          inline_string: "envoy.wasm.null.stackdriver"
      configuration: >-
        {}`

const inboundStackdriverFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          inline_string: "envoy.wasm.metadata_exchange"
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stackdriver_inbound"
      vm_config:
        vm_id: "stackdriver_inbound"
        runtime: "envoy.wasm.runtime.null"
        code:
          inline_string: "envoy.wasm.null.stackdriver"
      configuration: >-
        {}`

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
 "gcp_gke_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_location": "us-east4-b"
},
"LABELS": {
 "app": "productpage",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "productpage-v1-84975bc778-pxz2w",
"STACKDRIVER_MONITORING_ENDPOINT": "localhost:12312",
"STACKDRIVER_LOGGING_ENDPOINT": "localhost:12312",`

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
 "gcp_gke_cluster_name": "test-cluster",
 "gcp_project": "test-project",
 "gcp_location": "us-east4-b"
},
"LABELS": {
 "app": "ratings",
 "version": "v1",
 "pod-template-hash": "84975bc778"
},
"ISTIO_PROXY_SHA": "istio-proxy:47e4559b8e4f0d516c0d17b233d127a3deb3d7ce",
"NAME": "ratings-v1-84975bc778-pxz2w",
"STACKDRIVER_MONITORING_ENDPOINT": "localhost:12312",
"STACKDRIVER_LOGGING_ENDPOINT": "localhost:12312",`

func compareTimeSeries(got, want *monitoringpb.TimeSeries) error {
	// ignore time difference
	got.Points[0].Interval = nil
	if !proto.Equal(want, got) {
		return fmt.Errorf("request count timeseries is not expected, got %v \nwant %v\n", proto.MarshalTextString(got), proto.MarshalTextString(want))
	}
	return nil
}

func compareLogEntries(got, want *logging.WriteLogEntriesRequest) error {
	for _, l := range got.Entries {
		l.Timestamp = nil
	}
	if !proto.Equal(want, got) {
		return fmt.Errorf("log entries are not expected, got %v \nwant %v\n", proto.MarshalTextString(got), proto.MarshalTextString(want))
	}
	return nil
}

func verifyCreateTimeSeriesReq(got *monitoringpb.CreateTimeSeriesRequest) (error, bool) {
	var srvReqCount, cltReqCount monitoringpb.TimeSeries
	p := &driver.Params{
		Vars: map[string]string{
			"ServerPort": "20019",
			"ClientPort": "20016",
		},
	}
	client, _ := p.Fill(fs.ClientRequestCountJSON)
	server, _ := p.Fill(fs.ServerRequestCountJSON)
	jsonpb.UnmarshalString(server, &srvReqCount)
	jsonpb.UnmarshalString(client, &cltReqCount)
	isClient := true
	for _, t := range got.TimeSeries {
		if t.Metric.Type == srvReqCount.Metric.Type {
			isClient = false
			return compareTimeSeries(t, &srvReqCount), isClient
		}
		if t.Metric.Type == cltReqCount.Metric.Type {
			return compareTimeSeries(t, &cltReqCount), isClient
		}
	}
	// at least one time series should match either client side request count or server side request count.
	return fmt.Errorf("cannot find expected request count from creat time series request %v", got), isClient
}

func verifyWriteLogEntriesReq(got *logging.WriteLogEntriesRequest) error {
	var srvLogReq logging.WriteLogEntriesRequest
	jsonpb.UnmarshalString(fs.ServerAccessLogJSON, &srvLogReq)
	return compareLogEntries(got, &srvLogReq)
}

func TestStackdriverPlugin(t *testing.T) {
	s := env.NewClientServerEnvoyTestSetup(env.StackdriverPluginTest, t)
	fsdm, fsdl := fs.NewFakeStackdriver(12312, 0)
	s.SetFiltersBeforeEnvoyRouterInClientToProxy(outboundStackdriverFilter)
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(inboundStackdriverFilter)
	s.SetServerNodeMetadata(inboundNodeMetadata)
	s.SetClientNodeMetadata(outboundNodeMetadata)
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}
	defer s.TearDownClientServerEnvoy()

	url := fmt.Sprintf("http://127.0.0.1:%d/echo", s.Ports().AppToClientProxyPort)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	for i := 0; i < 10; i++ {
		if _, _, err := env.HTTPGet(url); err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
	}
	srvMetricRcv := false
	cltMetricRcv := false

	for i := 0; i < 3; i++ {
		// Two requests should be recevied by monitoring server: one from client and one from server.
		// One request should be received by logging server.
		select {
		case req := <-fsdm.RcvMetricReq:
			err, isClient := verifyCreateTimeSeriesReq(req)
			if err != nil {
				t.Errorf("CreateTimeSeries verification failed: %v", err)
			}
			if isClient {
				cltMetricRcv = true
			} else {
				srvMetricRcv = true
			}
		case req := <-fsdl.RcvLoggingReq:
			if err := verifyWriteLogEntriesReq(req); err != nil {
				t.Errorf("WriteLogEntries verification failed: %v", err)
			}
		case <-time.After(20 * time.Second):
			t.Error("timeout on waiting Stackdriver server to receive request")
		}
	}
	if !srvMetricRcv || !cltMetricRcv {
		t.Errorf("fail to receive metric request from both sides. client recieved: %v and server recieved: %v", cltMetricRcv, srvMetricRcv)
	}
}
