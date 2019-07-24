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

	grpc "google.golang.org/grpc"

	empty "github.com/golang/protobuf/ptypes/empty"
	"google.golang.org/genproto/googleapis/api/metric"
	"google.golang.org/genproto/googleapis/api/monitoredres"
	monitoringpb "google.golang.org/genproto/googleapis/monitoring/v3"
)

// FakeStackdriverServer is a fake stackdriver server which implements all of monitoring v3 service method.
type FakeStackdriverServer struct {
	RcvReq chan *monitoringpb.CreateTimeSeriesRequest
}

// ListMonitoredResourceDescriptors implements ListMonitoredResourceDescriptors method.
func (s *FakeStackdriverServer) ListMonitoredResourceDescriptors(context.Context, *monitoringpb.ListMonitoredResourceDescriptorsRequest) (*monitoringpb.ListMonitoredResourceDescriptorsResponse, error) {
	return &monitoringpb.ListMonitoredResourceDescriptorsResponse{}, nil
}

// GetMonitoredResourceDescriptor implements GetMonitoredResourceDescriptor method.
func (s *FakeStackdriverServer) GetMonitoredResourceDescriptor(context.Context, *monitoringpb.GetMonitoredResourceDescriptorRequest) (*monitoredres.MonitoredResourceDescriptor, error) {
	return &monitoredres.MonitoredResourceDescriptor{}, nil
}

// ListMetricDescriptors implements ListMetricDescriptors method.
func (s *FakeStackdriverServer) ListMetricDescriptors(context.Context, *monitoringpb.ListMetricDescriptorsRequest) (*monitoringpb.ListMetricDescriptorsResponse, error) {
	return &monitoringpb.ListMetricDescriptorsResponse{}, nil
}

// GetMetricDescriptor implements GetMetricDescriptor method.
func (s *FakeStackdriverServer) GetMetricDescriptor(context.Context, *monitoringpb.GetMetricDescriptorRequest) (*metric.MetricDescriptor, error) {
	return &metric.MetricDescriptor{}, nil
}

// CreateMetricDescriptor implements CreateMetricDescriptor method.
func (s *FakeStackdriverServer) CreateMetricDescriptor(_ context.Context, req *monitoringpb.CreateMetricDescriptorRequest) (*metric.MetricDescriptor, error) {
	return &metric.MetricDescriptor{}, nil
}

// DeleteMetricDescriptor implements DeleteMetricDescriptor method.
func (s *FakeStackdriverServer) DeleteMetricDescriptor(context.Context, *monitoringpb.DeleteMetricDescriptorRequest) (*empty.Empty, error) {
	return &empty.Empty{}, nil
}

// ListTimeSeries implements ListTimeSeries method.
func (s *FakeStackdriverServer) ListTimeSeries(context.Context, *monitoringpb.ListTimeSeriesRequest) (*monitoringpb.ListTimeSeriesResponse, error) {
	return &monitoringpb.ListTimeSeriesResponse{}, nil
}

// CreateTimeSeries implements CreateTimeSeries method.
func (s *FakeStackdriverServer) CreateTimeSeries(ctx context.Context, req *monitoringpb.CreateTimeSeriesRequest) (*empty.Empty, error) {
	s.RcvReq <- req
	return &empty.Empty{}, nil
}

func newServer() *FakeStackdriverServer {
	return &FakeStackdriverServer{}
}

// NewFakeStackdriver ...
func NewFakeStackdriver(port uint16) *FakeStackdriverServer {
	log.Printf("Stackdriver server listening on port %v\n", port)
	grpcServer := grpc.NewServer()
	fsds := &FakeStackdriverServer{
		RcvReq: make(chan *monitoringpb.CreateTimeSeriesRequest, 2),
	}
	monitoringpb.RegisterMetricServiceServer(grpcServer, fsds)

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
	return fsds
}
