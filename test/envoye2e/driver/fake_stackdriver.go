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

package driver

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/http"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"

	edgespb "cloud.google.com/go/meshtelemetry/v1alpha1"
	jsonpb "github.com/golang/protobuf/jsonpb"
	proto "github.com/golang/protobuf/proto"
	empty "github.com/golang/protobuf/ptypes/empty"
	"google.golang.org/genproto/googleapis/api/metric"
	"google.golang.org/genproto/googleapis/api/monitoredres"
	logging "google.golang.org/genproto/googleapis/logging/v2"
	monitoringpb "google.golang.org/genproto/googleapis/monitoring/v3"
)

// MetricServer is a fake stackdriver server which implements all of monitoring v3 service method.
type MetricServer struct {
	delay        time.Duration
	listTsResp   monitoringpb.ListTimeSeriesResponse
	RcvMetricReq chan *monitoringpb.CreateTimeSeriesRequest
	mux          sync.Mutex
}

// LoggingServer is a fake stackdriver server which implements all of logging v2 service method.
type LoggingServer struct {
	delay            time.Duration
	listLogEntryResp logging.ListLogEntriesResponse
	RcvLoggingReq    chan *logging.WriteLogEntriesRequest
	mux              sync.Mutex
}

// MeshEdgesServiceServer is a fake stackdriver server which implements all of mesh edge service method.
type MeshEdgesServiceServer struct {
	delay                   time.Duration
	RcvTrafficAssertionsReq chan *edgespb.ReportTrafficAssertionsRequest
}

// ListMonitoredResourceDescriptors implements ListMonitoredResourceDescriptors method.
func (s *MetricServer) ListMonitoredResourceDescriptors(
	context.Context, *monitoringpb.ListMonitoredResourceDescriptorsRequest) (
	*monitoringpb.ListMonitoredResourceDescriptorsResponse, error) {
	return &monitoringpb.ListMonitoredResourceDescriptorsResponse{}, nil
}

// GetMonitoredResourceDescriptor implements GetMonitoredResourceDescriptor method.
func (s *MetricServer) GetMonitoredResourceDescriptor(
	context.Context, *monitoringpb.GetMonitoredResourceDescriptorRequest) (
	*monitoredres.MonitoredResourceDescriptor, error) {
	return &monitoredres.MonitoredResourceDescriptor{}, nil
}

// ListMetricDescriptors implements ListMetricDescriptors method.
func (s *MetricServer) ListMetricDescriptors(
	context.Context, *monitoringpb.ListMetricDescriptorsRequest) (
	*monitoringpb.ListMetricDescriptorsResponse, error) {
	return &monitoringpb.ListMetricDescriptorsResponse{}, nil
}

// GetMetricDescriptor implements GetMetricDescriptor method.
func (s *MetricServer) GetMetricDescriptor(
	context.Context, *monitoringpb.GetMetricDescriptorRequest) (
	*metric.MetricDescriptor, error) {
	return &metric.MetricDescriptor{}, nil
}

// CreateMetricDescriptor implements CreateMetricDescriptor method.
func (s *MetricServer) CreateMetricDescriptor(_ context.Context, req *monitoringpb.CreateMetricDescriptorRequest) (*metric.MetricDescriptor, error) {
	return &metric.MetricDescriptor{}, nil
}

// DeleteMetricDescriptor implements DeleteMetricDescriptor method.
func (s *MetricServer) DeleteMetricDescriptor(context.Context, *monitoringpb.DeleteMetricDescriptorRequest) (*empty.Empty, error) {
	return &empty.Empty{}, nil
}

// ListTimeSeries implements ListTimeSeries method.
func (s *MetricServer) ListTimeSeries(context.Context, *monitoringpb.ListTimeSeriesRequest) (*monitoringpb.ListTimeSeriesResponse, error) {
	s.mux.Lock()
	defer s.mux.Unlock()
	return &s.listTsResp, nil
}

// CreateTimeSeries implements CreateTimeSeries method.
func (s *MetricServer) CreateTimeSeries(ctx context.Context, req *monitoringpb.CreateTimeSeriesRequest) (*empty.Empty, error) {
	log.Printf("receive CreateTimeSeriesRequest %+v", *req)
	s.mux.Lock()
	defer s.mux.Unlock()
	s.listTsResp.TimeSeries = append(s.listTsResp.TimeSeries, req.TimeSeries...)
	s.RcvMetricReq <- req
	time.Sleep(s.delay)
	return &empty.Empty{}, nil
}

// DeleteLog implements DeleteLog method.
func (s *LoggingServer) DeleteLog(context.Context, *logging.DeleteLogRequest) (*empty.Empty, error) {
	return &empty.Empty{}, nil
}

// WriteLogEntries implements WriteLogEntries method.
func (s *LoggingServer) WriteLogEntries(ctx context.Context, req *logging.WriteLogEntriesRequest) (*logging.WriteLogEntriesResponse, error) {
	log.Printf("receive WriteLogEntriesRequest %+v", *req)
	s.mux.Lock()
	defer s.mux.Unlock()
	for _, entry := range req.Entries {
		// Add the general labels to every log entry in list logentries response.
		tmpEntry := proto.Clone(entry).(*logging.LogEntry)
		for k, v := range req.Labels {
			tmpEntry.Labels[k] = v
		}
		s.listLogEntryResp.Entries = append(s.listLogEntryResp.Entries, tmpEntry)
	}
	s.RcvLoggingReq <- req
	time.Sleep(s.delay)
	return &logging.WriteLogEntriesResponse{}, nil
}

// ListLogEntries implements ListLogEntries method.
func (s *LoggingServer) ListLogEntries(context.Context, *logging.ListLogEntriesRequest) (*logging.ListLogEntriesResponse, error) {
	s.mux.Lock()
	defer s.mux.Unlock()
	return &s.listLogEntryResp, nil
}

// ListLogs implements ListLogs method.
func (s *LoggingServer) ListLogs(context.Context, *logging.ListLogsRequest) (*logging.ListLogsResponse, error) {
	return &logging.ListLogsResponse{}, nil
}

// ListMonitoredResourceDescriptors immplements ListMonitoredResourceDescriptors method.
func (s *LoggingServer) ListMonitoredResourceDescriptors(
	context.Context, *logging.ListMonitoredResourceDescriptorsRequest) (
	*logging.ListMonitoredResourceDescriptorsResponse, error) {
	return &logging.ListMonitoredResourceDescriptorsResponse{}, nil
}

// ReportTrafficAssertions is defined by the Mesh Edges Service.
func (e *MeshEdgesServiceServer) ReportTrafficAssertions(
	ctx context.Context, req *edgespb.ReportTrafficAssertionsRequest) (
	*edgespb.ReportTrafficAssertionsResponse, error) {
	log.Printf("receive ReportTrafficAssertionsRequest %+v", *req)
	e.RcvTrafficAssertionsReq <- req
	time.Sleep(e.delay)
	return &edgespb.ReportTrafficAssertionsResponse{}, nil
}

// GetTimeSeries returns all received time series in a ListTimeSeriesResponse as a marshaled json string
func (s *MetricServer) GetTimeSeries(w http.ResponseWriter, req *http.Request) {
	s.mux.Lock()
	defer s.mux.Unlock()
	var m jsonpb.Marshaler
	if s, err := m.MarshalToString(&s.listTsResp); err != nil {
		fmt.Fprintln(w, "Fail to marshal received time series")
	} else {
		fmt.Fprintln(w, s)
	}
}

// GetLogEntries returns all received log entries in a ReportTrafficAssertionsRequest as a marshaled json string.
func (s *LoggingServer) GetLogEntries(w http.ResponseWriter, req *http.Request) {
	s.mux.Lock()
	defer s.mux.Unlock()
	var m jsonpb.Marshaler
	if s, err := m.MarshalToString(&s.listLogEntryResp); err != nil {
		fmt.Fprintln(w, "Fail to marshal received log entries")
	} else {
		fmt.Fprintln(w, s)
	}
}

// NewFakeStackdriver creates a new fake Stackdriver server.
func NewFakeStackdriver(port uint16, delay time.Duration,
	enableTLS bool, bearer string) (*MetricServer, *LoggingServer, *MeshEdgesServiceServer, *grpc.Server) {
	log.Printf("Stackdriver server listening on port %v\n", port)

	var options []grpc.ServerOption
	if enableTLS {
		creds, err := credentials.NewServerTLSFromFile(
			TestPath("testdata/certs/stackdriver.pem"),
			TestPath("testdata/certs/stackdriver.key"))
		if err != nil {
			log.Fatalf("failed to read certificate: %v", err)
		}
		options = append(options, grpc.Creds(creds))
	}
	if bearer != "" {
		options = append(options, grpc.UnaryInterceptor(
			func(ctx context.Context, req interface{},
				_ *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
				md, ok := metadata.FromIncomingContext(ctx)
				if !ok {
					return nil, fmt.Errorf("missing metadata, want %q and x-goog-user-project", bearer)
				}
				if got := md["authorization"]; len(got) != 1 || got[0] != fmt.Sprintf("Bearer %s", bearer) {
					return nil, fmt.Errorf("authorization failure: got %q, want %q", got, bearer)
				}
				if got := md["x-goog-user-project"]; len(got) != 1 || got[0] != "test-project" {
					return nil, fmt.Errorf("x-goog-user-project failure: got %q, want test-project", got)
				}
				return handler(ctx, req)
			}))
	}
	grpcServer := grpc.NewServer(options...)

	fsdms := &MetricServer{
		delay:        delay,
		RcvMetricReq: make(chan *monitoringpb.CreateTimeSeriesRequest, 2),
	}
	fsdls := &LoggingServer{
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
	return fsdms, fsdls, edgesSvc, grpcServer
}

func RunFakeStackdriver(port uint16) error {
	grpcServer := grpc.NewServer()
	fsdms := &MetricServer{
		RcvMetricReq: make(chan *monitoringpb.CreateTimeSeriesRequest, 100),
	}
	fsdls := &LoggingServer{
		RcvLoggingReq: make(chan *logging.WriteLogEntriesRequest, 100),
	}
	edgesSvc := &MeshEdgesServiceServer{
		RcvTrafficAssertionsReq: make(chan *edgespb.ReportTrafficAssertionsRequest, 100),
	}
	monitoringpb.RegisterMetricServiceServer(grpcServer, fsdms)
	logging.RegisterLoggingServiceV2Server(grpcServer, fsdls)
	edgespb.RegisterMeshEdgesServiceServer(grpcServer, edgesSvc)

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	http.HandleFunc("/timeseries", fsdms.GetTimeSeries)
	http.HandleFunc("/logentries", fsdls.GetLogEntries)
	go func() {
		// start an http endpoint to serve time series in json text
		log.Fatal(http.ListenAndServe(fmt.Sprintf(":%d", port+1), nil))
	}()
	return grpcServer.Serve(lis)
}
