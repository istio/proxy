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

package fakestackdriver

import (
	"context"
	"fmt"
	"log"
	"net"
	"time"

	grpc "google.golang.org/grpc"

	edgespb "cloud.google.com/go/meshtelemetry/v1alpha1"
	empty "github.com/golang/protobuf/ptypes/empty"
	"google.golang.org/genproto/googleapis/api/metric"
	"google.golang.org/genproto/googleapis/api/monitoredres"
	logging "google.golang.org/genproto/googleapis/logging/v2"
	monitoringpb "google.golang.org/genproto/googleapis/monitoring/v3"
)

// FakeStackdriverMetricServer is a fake stackdriver server which implements all of monitoring v3 service method.
type FakeStackdriverMetricServer struct {
	delay        time.Duration
	RcvMetricReq chan *monitoringpb.CreateTimeSeriesRequest
}

// FakeStackdriverLoggingServer is a fake stackdriver server which implements all of logging v2 service method.
type FakeStackdriverLoggingServer struct {
	delay         time.Duration
	RcvLoggingReq chan *logging.WriteLogEntriesRequest
}

type MeshEdgesServiceServer struct {
	delay                   time.Duration
	RcvTrafficAssertionsReq chan *edgespb.ReportTrafficAssertionsRequest
}

// ListMonitoredResourceDescriptors implements ListMonitoredResourceDescriptors method.
func (s *FakeStackdriverMetricServer) ListMonitoredResourceDescriptors(context.Context, *monitoringpb.ListMonitoredResourceDescriptorsRequest) (*monitoringpb.ListMonitoredResourceDescriptorsResponse, error) {
	return &monitoringpb.ListMonitoredResourceDescriptorsResponse{}, nil
}

// GetMonitoredResourceDescriptor implements GetMonitoredResourceDescriptor method.
func (s *FakeStackdriverMetricServer) GetMonitoredResourceDescriptor(context.Context, *monitoringpb.GetMonitoredResourceDescriptorRequest) (*monitoredres.MonitoredResourceDescriptor, error) {
	return &monitoredres.MonitoredResourceDescriptor{}, nil
}

// ListMetricDescriptors implements ListMetricDescriptors method.
func (s *FakeStackdriverMetricServer) ListMetricDescriptors(context.Context, *monitoringpb.ListMetricDescriptorsRequest) (*monitoringpb.ListMetricDescriptorsResponse, error) {
	return &monitoringpb.ListMetricDescriptorsResponse{}, nil
}

// GetMetricDescriptor implements GetMetricDescriptor method.
func (s *FakeStackdriverMetricServer) GetMetricDescriptor(context.Context, *monitoringpb.GetMetricDescriptorRequest) (*metric.MetricDescriptor, error) {
	return &metric.MetricDescriptor{}, nil
}

// CreateMetricDescriptor implements CreateMetricDescriptor method.
func (s *FakeStackdriverMetricServer) CreateMetricDescriptor(_ context.Context, req *monitoringpb.CreateMetricDescriptorRequest) (*metric.MetricDescriptor, error) {
	return &metric.MetricDescriptor{}, nil
}

// DeleteMetricDescriptor implements DeleteMetricDescriptor method.
func (s *FakeStackdriverMetricServer) DeleteMetricDescriptor(context.Context, *monitoringpb.DeleteMetricDescriptorRequest) (*empty.Empty, error) {
	return &empty.Empty{}, nil
}

// ListTimeSeries implements ListTimeSeries method.
func (s *FakeStackdriverMetricServer) ListTimeSeries(context.Context, *monitoringpb.ListTimeSeriesRequest) (*monitoringpb.ListTimeSeriesResponse, error) {
	return &monitoringpb.ListTimeSeriesResponse{}, nil
}

// CreateTimeSeries implements CreateTimeSeries method.
func (s *FakeStackdriverMetricServer) CreateTimeSeries(ctx context.Context, req *monitoringpb.CreateTimeSeriesRequest) (*empty.Empty, error) {
	s.RcvMetricReq <- req
	time.Sleep(s.delay)
	return &empty.Empty{}, nil
}

// DeleteLog implements DeleteLog method.
func (s *FakeStackdriverLoggingServer) DeleteLog(context.Context, *logging.DeleteLogRequest) (*empty.Empty, error) {
	return &empty.Empty{}, nil
}

// WriteLogEntries implements WriteLogEntries method.
func (s *FakeStackdriverLoggingServer) WriteLogEntries(ctx context.Context, req *logging.WriteLogEntriesRequest) (*logging.WriteLogEntriesResponse, error) {
	s.RcvLoggingReq <- req
	time.Sleep(s.delay)
	return &logging.WriteLogEntriesResponse{}, nil
}

// ListLogEntries implementes ListLogEntries method.
func (s *FakeStackdriverLoggingServer) ListLogEntries(context.Context, *logging.ListLogEntriesRequest) (*logging.ListLogEntriesResponse, error) {
	return &logging.ListLogEntriesResponse{}, nil
}

// ListLogs implements ListLogs method.
func (s *FakeStackdriverLoggingServer) ListLogs(context.Context, *logging.ListLogsRequest) (*logging.ListLogsResponse, error) {
	return &logging.ListLogsResponse{}, nil
}

// ListMonitoredResourceDescriptors immplements ListMonitoredResourceDescriptors method.
func (s *FakeStackdriverLoggingServer) ListMonitoredResourceDescriptors(context.Context, *logging.ListMonitoredResourceDescriptorsRequest) (*logging.ListMonitoredResourceDescriptorsResponse, error) {
	return &logging.ListMonitoredResourceDescriptorsResponse{}, nil
}

// ReportTrafficAssertions is defined by the Mesh Edges Service.
func (e *MeshEdgesServiceServer) ReportTrafficAssertions(ctx context.Context, req *edgespb.ReportTrafficAssertionsRequest) (*edgespb.ReportTrafficAssertionsResponse, error) {
	e.RcvTrafficAssertionsReq <- req
	time.Sleep(e.delay)
	return &edgespb.ReportTrafficAssertionsResponse{}, nil
}

// NewFakeStackdriver creates a new fake Stackdriver server.
func NewFakeStackdriver(port uint16, delay time.Duration) (*FakeStackdriverMetricServer, *FakeStackdriverLoggingServer, *MeshEdgesServiceServer) {
	log.Printf("Stackdriver server listening on port %v\n", port)
	grpcServer := grpc.NewServer()
	fsdms := &FakeStackdriverMetricServer{
		delay:        delay,
		RcvMetricReq: make(chan *monitoringpb.CreateTimeSeriesRequest, 2),
	}
	fsdls := &FakeStackdriverLoggingServer{
		delay:         delay,
		RcvLoggingReq: make(chan *logging.WriteLogEntriesRequest, 2),
	}
	edgesSvc := &MeshEdgesServiceServer{
		delay:                   delay,
		RcvTrafficAssertionsReq: make(chan *edgespb.ReportTrafficAssertionsRequest, 2),
	}
	monitoringpb.RegisterMetricServiceServer(grpcServer, fsdms)
	logging.RegisterLoggingServiceV2Server(grpcServer, fsdls)
	edgespb.RegisterMeshEdgesServiceServer(grpcServer, edgesSvc)

	go func() {
		lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
		if err != nil {
			log.Fatalf("failed to listen: %v", err)
		}
		err = grpcServer.Serve(lis)
		if err != nil {
			log.Fatalf("fake stackdriver server terminated abnormally: %v", err)
		}
	}()
	return fsdms, fsdls, edgesSvc
}
