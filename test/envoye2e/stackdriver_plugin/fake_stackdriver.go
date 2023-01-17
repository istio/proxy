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

package stackdriverplugin

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"cloud.google.com/go/logging/apiv2/loggingpb"
	"cloud.google.com/go/monitoring/apiv3/v2/monitoringpb"
	cloudtracev1 "cloud.google.com/go/trace/apiv1/tracepb"
	cloudtracev2 "cloud.google.com/go/trace/apiv2/tracepb"
	"github.com/golang/protobuf/jsonpb" // nolint: depguard // We need the deprecated module since the jsonpb replacement is not backwards compatible.
	"github.com/golang/protobuf/ptypes/empty"
	"google.golang.org/genproto/googleapis/api/metric"
	"google.golang.org/genproto/googleapis/api/monitoredres"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/emptypb"

	"istio.io/proxy/test/envoye2e/driver"
)

// MetricServer is a fake stackdriver server which implements all monitoring v3 service methods.
type MetricServer struct {
	delay        time.Duration
	listTSResp   monitoringpb.ListTimeSeriesResponse
	RcvMetricReq chan *monitoringpb.CreateTimeSeriesRequest
	mux          sync.Mutex
}

// LoggingServer is a fake stackdriver server which implements all logging v2 service methods.
type LoggingServer struct {
	delay            time.Duration
	listLogEntryResp loggingpb.ListLogEntriesResponse
	RcvLoggingReq    chan *loggingpb.WriteLogEntriesRequest
	mux              sync.Mutex
}

// TracesServer is a fake stackdriver server which implements all cloudtrace v1 service methods.
type TracesServer struct {
	delay          time.Duration
	listTracesResp cloudtracev1.ListTracesResponse
	RcvTracesReq   chan *cloudtracev2.BatchWriteSpansRequest
	mux            sync.Mutex
	traceMap       map[string]*cloudtracev1.Trace
}

// ListMonitoredResourceDescriptors implements ListMonitoredResourceDescriptors method.
func (s *MetricServer) ListMonitoredResourceDescriptors(
	context.Context, *monitoringpb.ListMonitoredResourceDescriptorsRequest) (
	*monitoringpb.ListMonitoredResourceDescriptorsResponse, error,
) {
	return &monitoringpb.ListMonitoredResourceDescriptorsResponse{}, nil
}

// GetMonitoredResourceDescriptor implements GetMonitoredResourceDescriptor method.
func (s *MetricServer) GetMonitoredResourceDescriptor(
	context.Context, *monitoringpb.GetMonitoredResourceDescriptorRequest) (
	*monitoredres.MonitoredResourceDescriptor, error,
) {
	return &monitoredres.MonitoredResourceDescriptor{}, nil
}

// ListMetricDescriptors implements ListMetricDescriptors method.
func (s *MetricServer) ListMetricDescriptors(
	context.Context, *monitoringpb.ListMetricDescriptorsRequest) (
	*monitoringpb.ListMetricDescriptorsResponse, error,
) {
	return &monitoringpb.ListMetricDescriptorsResponse{}, nil
}

// GetMetricDescriptor implements GetMetricDescriptor method.
func (s *MetricServer) GetMetricDescriptor(
	context.Context, *monitoringpb.GetMetricDescriptorRequest) (
	*metric.MetricDescriptor, error,
) {
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
	return &s.listTSResp, nil
}

// CreateTimeSeries implements CreateTimeSeries method.
func (s *MetricServer) CreateTimeSeries(ctx context.Context, req *monitoringpb.CreateTimeSeriesRequest) (*empty.Empty, error) {
	log.Printf("receive CreateTimeSeriesRequest %v", req.String())
	s.mux.Lock()
	defer s.mux.Unlock()
	s.listTSResp.TimeSeries = append(s.listTSResp.TimeSeries, req.TimeSeries...)
	s.RcvMetricReq <- req
	time.Sleep(s.delay)
	return &empty.Empty{}, nil
}

func (s *MetricServer) CreateServiceTimeSeries(ctx context.Context, request *monitoringpb.CreateTimeSeriesRequest) (*emptypb.Empty, error) {
	return s.CreateTimeSeries(ctx, request)
}

// DeleteLog implements DeleteLog method.
func (s *LoggingServer) DeleteLog(context.Context, *loggingpb.DeleteLogRequest) (*empty.Empty, error) {
	return &empty.Empty{}, nil
}

// WriteLogEntries implements WriteLogEntries method.
func (s *LoggingServer) WriteLogEntries(ctx context.Context, req *loggingpb.WriteLogEntriesRequest) (*loggingpb.WriteLogEntriesResponse, error) {
	log.Printf("receive WriteLogEntriesRequest %v", req.String())
	s.mux.Lock()
	defer s.mux.Unlock()
	for _, entry := range req.Entries {
		// Add the general labels to every log entry in list logentries response.
		tmpEntry := proto.Clone(entry).(*loggingpb.LogEntry)
		for k, v := range req.Labels {
			tmpEntry.Labels[k] = v
		}
		// Set per entry log name.
		tmpEntry.LogName = req.LogName
		s.listLogEntryResp.Entries = append(s.listLogEntryResp.Entries, tmpEntry)
	}
	s.RcvLoggingReq <- req
	time.Sleep(s.delay)
	return &loggingpb.WriteLogEntriesResponse{}, nil
}

// ListLogEntries implements ListLogEntries method.
func (s *LoggingServer) ListLogEntries(context.Context, *loggingpb.ListLogEntriesRequest) (*loggingpb.ListLogEntriesResponse, error) {
	s.mux.Lock()
	defer s.mux.Unlock()
	return &s.listLogEntryResp, nil
}

// ListLogs implements ListLogs method.
func (s *LoggingServer) ListLogs(context.Context, *loggingpb.ListLogsRequest) (*loggingpb.ListLogsResponse, error) {
	return &loggingpb.ListLogsResponse{}, nil
}

// ListMonitoredResourceDescriptors immplements ListMonitoredResourceDescriptors method.
func (s *LoggingServer) ListMonitoredResourceDescriptors(
	context.Context, *loggingpb.ListMonitoredResourceDescriptorsRequest) (
	*loggingpb.ListMonitoredResourceDescriptorsResponse, error,
) {
	return &loggingpb.ListMonitoredResourceDescriptorsResponse{}, nil
}

func (s *LoggingServer) TailLogEntries(server loggingpb.LoggingServiceV2_TailLogEntriesServer) error {
	panic("TailLogEntries: implement me")
}

// GetTimeSeries returns all received time series in a ListTimeSeriesResponse as a marshaled json string
func (s *MetricServer) GetTimeSeries(w http.ResponseWriter, req *http.Request) {
	s.mux.Lock()
	defer s.mux.Unlock()
	var m jsonpb.Marshaler
	if s, err := m.MarshalToString(&s.listTSResp); err != nil {
		fmt.Fprintln(w, "Fail to marshal received time series")
	} else {
		fmt.Fprintln(w, s)
	}
}

// GetLogEntries returns all received log entries in a ListLogEntriesResponse as a marshaled json string.
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

// ListTraces implements ListTraces method.
func (s *TracesServer) ListTraces(ctx context.Context, req *cloudtracev1.ListTracesRequest) (*cloudtracev1.ListTracesResponse, error) {
	s.mux.Lock()
	defer s.mux.Unlock()
	numTracesAdded := 0
	for _, trace := range s.traceMap {
		if req.ProjectId != trace.ProjectId {
			continue
		}
		foundSpan := false
		for _, span := range trace.Spans {
			// If any span started before request start time or ended after request start time, we skip adding this trace.
			if req.StartTime.Seconds > span.StartTime.Seconds || req.EndTime.Seconds < span.EndTime.Seconds {
				foundSpan = false
				break
			}
			// This does label matching to find any span that match the filter.
			if req.Filter != "" {
				keyValue := strings.Split(req.Filter, ":")
				key := keyValue[0]
				exactMatch := false
				if strings.HasPrefix(key, "+") {
					key = strings.TrimPrefix(key, "+")
					exactMatch = true
				}
				for k, v := range span.Labels {
					if k == key && ((exactMatch && v == keyValue[1]) || (!exactMatch && strings.HasPrefix(v, keyValue[1]))) {
						foundSpan = true
						continue
					}
				}
			} else {
				foundSpan = true
			}
		}
		if foundSpan {
			if (req.PageSize > 0 && int32(numTracesAdded) < req.PageSize) || req.PageSize <= 0 {
				s.listTracesResp.Traces = append(s.listTracesResp.Traces, trace)
				numTracesAdded++
			}
		}
	}
	return &s.listTracesResp, nil
}

// Traces returns the batch of Tracess reported to the server in the form
// of a JSON-serialized string of a ListTracesResponse proto.
func (s *TracesServer) Traces(w http.ResponseWriter, r *http.Request) {
	s.mux.Lock()
	var m jsonpb.Marshaler
	if err := m.Marshal(w, &s.listTracesResp); err != nil {
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
	}
	s.mux.Unlock()
}

// GetTrace implements GetTrace method.
func (s *TracesServer) GetTrace(context.Context, *cloudtracev1.GetTraceRequest) (*cloudtracev1.Trace, error) {
	log.Fatal("Unimplemented Method")
	return &cloudtracev1.Trace{}, nil
}

// GetTrace implements GetTrace method.
func (s *TracesServer) PatchTraces(context.Context, *cloudtracev1.PatchTracesRequest) (*empty.Empty, error) {
	log.Fatal("Unimplemented Method")
	return &empty.Empty{}, nil
}

func getID(id string) (uint64, error) {
	// Convert hexadecimal string to int64
	dec, err := strconv.ParseUint(id, 16, 64)
	if err != nil {
		return 0, err
	}
	return dec, nil
}

// BatchWriteSpans implements BatchWriteSpans method.
func (s *TracesServer) BatchWriteSpans(ctx context.Context, req *cloudtracev2.BatchWriteSpansRequest) (*empty.Empty, error) {
	log.Printf("receive BatchWriteSpansRequest %+v", req.String())
	s.mux.Lock()
	defer s.mux.Unlock()
	for _, span := range req.Spans {
		re := regexp.MustCompile(`projects\/([\w-]+)\/traces\/(\w+)\/spans\/(\w+)`)
		match := re.FindStringSubmatch(span.Name)
		if len(match) < 4 {
			log.Printf("span name not in correct format: %v", span.Name)
			continue
		}
		projectID := match[1]
		traceID := match[2]
		spanID, err := getID(match[3])
		if err != nil {
			log.Printf("Could not convert span id to int: %v", err)
			continue
		}
		parentSpanID, err := getID(span.ParentSpanId)
		if err != nil {
			log.Printf("Could not convert parent span id to int: %v", err)
			continue
		}

		newTraceSpan := &cloudtracev1.TraceSpan{
			SpanId:       spanID,
			Name:         span.DisplayName.GetValue(),
			ParentSpanId: parentSpanID,
			StartTime:    span.StartTime,
			EndTime:      span.EndTime,
			Labels:       make(map[string]string),
		}
		// Add Labels, so that test can query it using filters.
		if span.ParentSpanId == "" {
			newTraceSpan.Labels["root"] = span.DisplayName.GetValue()
		}

		for key, val := range span.Attributes.AttributeMap {
			newTraceSpan.Labels[key] = val.GetStringValue().Value
		}

		if existingTrace, ok := s.traceMap[traceID]; ok {
			existingTrace.Spans = append(existingTrace.Spans, newTraceSpan)
		} else {
			s.traceMap[traceID] = &cloudtracev1.Trace{
				ProjectId: projectID,
				TraceId:   traceID,
				Spans: []*cloudtracev1.TraceSpan{
					newTraceSpan,
				},
			}
			s.listTracesResp.Traces = append(s.listTracesResp.Traces, s.traceMap[traceID])
		}
	}

	s.RcvTracesReq <- req
	time.Sleep(s.delay)
	return &empty.Empty{}, nil
}

// CreateSpan implements CreateSpan method.
func (s *TracesServer) CreateSpan(ctx context.Context, req *cloudtracev2.Span) (*cloudtracev2.Span, error) {
	log.Fatal("Unimplemented Method")
	return &cloudtracev2.Span{}, nil
}

// NewFakeStackdriver creates a new fake Stackdriver server.
func NewFakeStackdriver(port uint16, delay time.Duration,
	enableTLS bool, bearer string,
) (*MetricServer, *LoggingServer, *TracesServer, *grpc.Server) {
	log.Printf("Stackdriver server listening on port %v\n", port)

	var options []grpc.ServerOption
	if enableTLS {
		creds, err := credentials.NewServerTLSFromFile(
			driver.TestPath("testdata/certs/stackdriver.pem"),
			driver.TestPath("testdata/certs/stackdriver.key"))
		if err != nil {
			log.Fatalf("failed to read certificate: %v", err)
		}
		options = append(options, grpc.Creds(creds))
	}
	if bearer != "" {
		options = append(options, grpc.UnaryInterceptor(
			func(ctx context.Context, req interface{},
				_ *grpc.UnaryServerInfo, handler grpc.UnaryHandler,
			) (interface{}, error) {
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
		RcvLoggingReq: make(chan *loggingpb.WriteLogEntriesRequest, 2),
	}
	traceSvc := &TracesServer{
		delay:        delay,
		RcvTracesReq: make(chan *cloudtracev2.BatchWriteSpansRequest, 2),
		traceMap:     make(map[string]*cloudtracev1.Trace),
	}
	monitoringpb.RegisterMetricServiceServer(grpcServer, fsdms)
	loggingpb.RegisterLoggingServiceV2Server(grpcServer, fsdls)
	cloudtracev1.RegisterTraceServiceServer(grpcServer, traceSvc)
	cloudtracev2.RegisterTraceServiceServer(grpcServer, traceSvc)

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
	return fsdms, fsdls, traceSvc, grpcServer
}

func RunFakeStackdriver(port uint16) error {
	grpcServer := grpc.NewServer()
	fsdms := &MetricServer{
		RcvMetricReq: make(chan *monitoringpb.CreateTimeSeriesRequest, 100),
	}
	fsdls := &LoggingServer{
		RcvLoggingReq: make(chan *loggingpb.WriteLogEntriesRequest, 100),
	}
	traceSvc := &TracesServer{
		RcvTracesReq: make(chan *cloudtracev2.BatchWriteSpansRequest, 100),
		traceMap:     make(map[string]*cloudtracev1.Trace),
	}

	// need something to chew through the channels to avoid deadlock when more
	// than 100 requests are received in testing
	go func() {
		for {
			select {
			case <-fsdms.RcvMetricReq:
				log.Printf("metric req received")
			case <-fsdls.RcvLoggingReq:
				log.Printf("log req received")
			case <-traceSvc.RcvTracesReq:
				log.Printf("trace req received")
			}
		}
	}()

	monitoringpb.RegisterMetricServiceServer(grpcServer, fsdms)
	loggingpb.RegisterLoggingServiceV2Server(grpcServer, fsdls)
	cloudtracev1.RegisterTraceServiceServer(grpcServer, traceSvc)
	cloudtracev2.RegisterTraceServiceServer(grpcServer, traceSvc)

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	http.HandleFunc("/timeseries", fsdms.GetTimeSeries)
	http.HandleFunc("/logentries", fsdls.GetLogEntries)
	http.HandleFunc("/traces", traceSvc.Traces)

	go func() {
		// start an http endpoint to serve responses in json text
		log.Fatal(http.ListenAndServe(fmt.Sprintf(":%d", port+1), nil))
	}()
	return grpcServer.Serve(lis)
}
