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
	"errors"
	"fmt"
	"testing"
	"time"

	"github.com/d4l3k/messagediff"

	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
	fs "istio.io/proxy/test/envoye2e/stackdriver_plugin/fake_stackdriver"

	edgespb "cloud.google.com/go/meshtelemetry/v1alpha1"
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
          local: { inline_string: "envoy.wasm.metadata_exchange" }
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stackdriver_outbound"
      vm_config:
        vm_id: "stackdriver_outbound"
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.null.stackdriver" }
      configuration: >-
        {}`

const inboundStackdriverFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.metadata_exchange" }
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stackdriver_inbound"
      vm_config:
        vm_id: "stackdriver_inbound"
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.null.stackdriver" }
      configuration: >-
        {
          "max_peer_cache_size": -1,
          "enableMeshEdgesReporting": "true",
          "meshEdgesReportingDuration": "1s"
        }`

const inboundStackdriverAndAccessLogFilter = `- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.metadata_exchange" }
      configuration: "test"
- name: envoy.filters.http.wasm
  config:
    config:
      vm_config:
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.access_log_policy" }
      configuration: >-
        {
          "log_window_duration": %s,
        }
- name: envoy.filters.http.wasm
  config:
    config:
      root_id: "stackdriver_inbound"
      vm_config:
        vm_id: "stackdriver_inbound"
        runtime: "envoy.wasm.runtime.null"
        code:
          local: { inline_string: "envoy.wasm.null.stackdriver" }
      configuration: >-
        {
          "max_peer_cache_size": -1,
          "enableMeshEdgesReporting": "true",
          "meshEdgesReportingDuration": "1s"
        }`

func compareTimeSeries(got, want *monitoringpb.TimeSeries) error {
	// ignore time difference
	got.Points[0].Interval = nil
	if !proto.Equal(want, got) {
		return fmt.Errorf("request count timeseries is not expected, got %v \nwant %v", proto.MarshalTextString(got), proto.MarshalTextString(want))
	}
	return nil
}

func compareLogEntries(got, want *logging.WriteLogEntriesRequest) error {
	for _, l := range got.Entries {
		l.Timestamp = nil
	}
	if !proto.Equal(want, got) {
		return fmt.Errorf("log entries are not expected, got %v \nwant %v", proto.MarshalTextString(got), proto.MarshalTextString(want))
	}
	return nil
}

func verifyCreateTimeSeriesReq(got *monitoringpb.CreateTimeSeriesRequest) (bool, error) {
	var srvReqCount, cltReqCount monitoringpb.TimeSeries
	p := &driver.Params{
		Vars: map[string]string{
			"ServerPort":                  "20043",
			"ClientPort":                  "20042",
			"ServiceAuthenticationPolicy": "NONE",
		},
	}
	p.LoadTestProto("testdata/stackdriver/client_request_count.yaml.tmpl", &cltReqCount)
	p.LoadTestProto("testdata/stackdriver/server_request_count.yaml.tmpl", &srvReqCount)
	isClient := true
	for _, t := range got.TimeSeries {
		if t.Metric.Type == srvReqCount.Metric.Type {
			isClient = false
			return isClient, compareTimeSeries(t, &srvReqCount)
		}
		if t.Metric.Type == cltReqCount.Metric.Type {
			return isClient, compareTimeSeries(t, &cltReqCount)
		}
	}
	// at least one time series should match either client side request count or server side request count.
	return isClient, fmt.Errorf("cannot find expected request count from creat time series request %v", got)
}

func verifyWriteLogEntriesReq(got *logging.WriteLogEntriesRequest) error {
	var srvLogReq logging.WriteLogEntriesRequest
	p := &driver.Params{
		Vars: map[string]string{
			"ServiceAuthenticationPolicy": "NONE",
		},
	}
	p.LoadTestProto("testdata/stackdriver/server_access_log.yaml.tmpl", &srvLogReq)
	return compareLogEntries(got, &srvLogReq)
}

var wantTrafficReq = &edgespb.ReportTrafficAssertionsRequest{
	Parent:  "projects/test-project",
	MeshUid: "mesh",
	TrafficAssertions: []*edgespb.TrafficAssertion{
		{
			Protocol:                    edgespb.TrafficAssertion_PROTOCOL_HTTP,
			DestinationServiceName:      "server",
			DestinationServiceNamespace: "default",
			Source: &edgespb.WorkloadInstance{
				Uid:               "kubernetes://productpage-v1-84975bc778-pxz2w.default",
				Location:          "us-east4-b",
				ClusterName:       "test-cluster",
				OwnerUid:          "kubernetes://apis/apps/v1/namespaces/default/deployments/productpage-v1",
				WorkloadName:      "productpage-v1",
				WorkloadNamespace: "default",
			},
			Destination: &edgespb.WorkloadInstance{
				Uid:               "kubernetes://ratings-v1-84975bc778-pxz2w.default",
				Location:          "us-east4-b",
				ClusterName:       "test-cluster",
				OwnerUid:          "kubernetes://apis/apps/v1/namespaces/default/deployments/ratings-v1",
				WorkloadName:      "ratings-v1",
				WorkloadNamespace: "default",
			},
		},
	},
}

func verifyTrafficAssertionsReq(got *edgespb.ReportTrafficAssertionsRequest) error {
	if s, same := messagediff.PrettyDiff(wantTrafficReq, got, messagediff.IgnoreStructField("Timestamp")); !same {
		return errors.New(s)
	}
	return nil
}

func setup(t *testing.T, inbound string) *env.TestSetup {
	s := env.NewClientServerEnvoyTestSetup(env.StackdriverPluginTest, t)

	if inbound == "" {
		inbound = inboundStackdriverFilter
	}
	s.SetFiltersBeforeEnvoyRouterInAppToClient(outboundStackdriverFilter)
	s.SetFiltersBeforeEnvoyRouterInProxyToServer(inbound)
	params := driver.Params{Vars: map[string]string{"SDPort": "12312"}}
	s.SetClientNodeMetadata(params.LoadTestData("testdata/client_node_metadata.json.tmpl"))
	s.SetServerNodeMetadata(params.LoadTestData("testdata/server_node_metadata.json.tmpl"))
	if err := s.SetUpClientServerEnvoy(); err != nil {
		t.Fatalf("Failed to setup test: %v", err)
	}

	return s
}

func issueGetRequests(port uint16, t *testing.T) {
	url := fmt.Sprintf("http://127.0.0.1:%d/echo", port)

	// Issues a GET echo request with 0 size body
	tag := "OKGet"
	for i := 0; i < 10; i++ {
		if _, _, err := env.HTTPGet(url); err != nil {
			t.Errorf("Failed in request %s: %v", tag, err)
		}
	}
}

func TestStackdriverPlugin(t *testing.T) {
	s := setup(t, "")
	defer s.TearDownClientServerEnvoy()
	fsdm, fsdl, edgesSvc, grpcServer := fs.NewFakeStackdriver(12312, 0)
	defer grpcServer.Stop()

	issueGetRequests(s.Ports().AppToClientProxyPort, t)

	srvMetricRcv := false
	cltMetricRcv := false
	logRcv := false
	edgeRcv := false

	to := time.NewTimer(20 * time.Second)

	for !(srvMetricRcv && cltMetricRcv && logRcv && edgeRcv) {
		select {
		case req := <-fsdm.RcvMetricReq:
			isClient, err := verifyCreateTimeSeriesReq(req)
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
			logRcv = true
		case req := <-edgesSvc.RcvTrafficAssertionsReq:
			if err := verifyTrafficAssertionsReq(req); err != nil {
				t.Errorf("ReportTrafficAssertions() verification failed: %v", err)
			}
			edgeRcv = true
		case <-to.C:
			to.Stop()
			rcv := fmt.Sprintf(
				"client metrics: %t, server metrics: %t, logs: %t, edges: %t",
				cltMetricRcv, srvMetricRcv, logRcv, edgeRcv,
			)
			t.Fatal("timeout: Stackdriver did not receive required requests: " + rcv)
		}
	}
}

func verifyNumberOfAccessLogs(fsdl *fs.LoggingServer, t *testing.T, expectedEntries int) {
	logRcv := false

	to := time.NewTimer(20 * time.Second)

	for !(logRcv) {
		select {
		case req := <-fsdl.RcvLoggingReq:
			if len(req.Entries) != expectedEntries {
				t.Errorf("WriteLogEntries verification failed. Number of entries expected: %v, got: %v", 1, len(req.Entries))
			}
			logRcv = true
		case <-to.C:
			to.Stop()
			rcv := fmt.Sprintf(
				"client logs: %t",
				logRcv,
			)
			t.Fatal("timeout: Stackdriver did not receive required requests: " + rcv)
		}
	}
}

func TestStackdriverAndAccessLogPlugin(t *testing.T) {
	s := setup(t, fmt.Sprintf(inboundStackdriverAndAccessLogFilter, "\"15s\""))
	defer s.TearDownClientServerEnvoy()
	_, fsdl, _, grpcServer := fs.NewFakeStackdriver(12312, 0)
	defer grpcServer.Stop()

	issueGetRequests(s.Ports().AppToClientProxyPort, t)
	verifyNumberOfAccessLogs(fsdl, t, 1)
}

func TestStackdriverAndAccessLogPluginLogRequestGetsLoggedAgain(t *testing.T) {
	s := setup(t, fmt.Sprintf(inboundStackdriverAndAccessLogFilter, "\"1s\""))
	defer s.TearDownClientServerEnvoy()
	_, fsdl, _, grpcServer := fs.NewFakeStackdriver(12312, 0)
	defer grpcServer.Stop()

	issueGetRequests(s.Ports().AppToClientProxyPort, t)
	// Sleep for one second
	time.Sleep(1 * time.Second)
	issueGetRequests(s.Ports().AppToClientProxyPort, t)

	verifyNumberOfAccessLogs(fsdl, t, 2)
}

func TestStackdriverAndAccessLogPluginAllErrorRequestsGetsLogged(t *testing.T) {
	s := setup(t, fmt.Sprintf(inboundStackdriverAndAccessLogFilter, "\"1s\""))
	defer s.TearDownClientServerEnvoy()
	_, fsdl, _, grpcServer := fs.NewFakeStackdriver(12312, 0)
	defer grpcServer.Stop()

	// Shuts down backend, so all 10 requests fail.
	s.StopHTTPBackend()
	issueGetRequests(s.Ports().AppToClientProxyPort, t)

	verifyNumberOfAccessLogs(fsdl, t, 10)
}
