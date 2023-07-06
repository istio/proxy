// Copyright Istio Authors
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
	"time"

	"github.com/golang/protobuf/proto"
	"github.com/google/go-cmp/cmp"
	collogspb "go.opentelemetry.io/proto/otlp/collector/logs/v1"
	colmetricspb "go.opentelemetry.io/proto/otlp/collector/metrics/v1"
	metricspb "go.opentelemetry.io/proto/otlp/metrics/v1"

	"google.golang.org/grpc"
	"google.golang.org/protobuf/testing/protocmp"
)

type OtelLogs struct {
	collogspb.UnimplementedLogsServiceServer
}

type OtelMetrics struct {
	colmetricspb.UnimplementedMetricsServiceServer
	finished bool
	verify   verify
}

type verify struct {
	want map[string]*metricspb.Metric
	done chan struct{}
}

type Otel struct {
	OtelLogs
	OtelMetrics
	grpc *grpc.Server
	Port uint16
	// Metrics contains the expected (total) metric protos. Client must produce it exactly.
	Metrics []string
}

func (x *OtelLogs) Export(ctx context.Context, req *collogspb.ExportLogsServiceRequest) (*collogspb.ExportLogsServiceResponse, error) {
	//log.Printf("log=%s\n", proto.MarshalTextString(req))
	return &collogspb.ExportLogsServiceResponse{}, nil
}

func (x *OtelMetrics) Export(ctx context.Context, req *colmetricspb.ExportMetricsServiceRequest) (*colmetricspb.ExportMetricsServiceResponse, error) {
	if x.finished {
		return &colmetricspb.ExportMetricsServiceResponse{}, nil
	}
	//log.Printf("metric=%s\n", proto.MarshalTextString(req))
	for _, rm := range req.ResourceMetrics {
		log.Printf("resource=%s\n", proto.MarshalTextString(rm.Resource))
		for _, sm := range rm.ScopeMetrics {
			log.Printf("scope=%s\n", proto.MarshalTextString(sm.Scope))
			for _, m := range sm.Metrics {
				if sum := m.GetSum(); sum != nil {
					for _, dp := range sum.DataPoints {
						dp.TimeUnixNano = 0
					}
				}
				if want, ok := x.verify.want[m.Name]; ok {
					if diff := cmp.Diff(m, want, protocmp.Transform()); diff != "" {
						log.Printf("mismatched metric: %v\n", diff)
					} else {
						delete(x.verify.want, m.Name)
					}
				}
			}
		}
	}
	if len(x.verify.want) == 0 {
		x.finished = true
		close(x.verify.done)
	}
	return &colmetricspb.ExportMetricsServiceResponse{}, nil
}

func (x *Otel) Wait() Step {
	return &x.verify
}

func (v *verify) Run(p *Params) error {
	d := 30 * time.Second
	select {
	case <-v.done:
		return nil
	case <-time.After(d):
		return fmt.Errorf("timed out waiting for all metrics to match")
	}
}
func (v *verify) Cleanup() {
}

var _ Step = &Otel{}

func (x *Otel) Run(p *Params) error {
	log.Printf("Otel server starting on %d\n", x.Port)
	x.grpc = grpc.NewServer()
	collogspb.RegisterLogsServiceServer(x.grpc, &x.OtelLogs)
	colmetricspb.RegisterMetricsServiceServer(x.grpc, &x.OtelMetrics)
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", x.Port))
	if err != nil {
		return err
	}
	x.verify.done = make(chan struct{})
	x.verify.want = make(map[string]*metricspb.Metric)
	for _, m := range x.Metrics {
		mpb := &metricspb.Metric{}
		p.LoadTestProto(m, mpb)
		x.verify.want[mpb.Name] = mpb
	}
	go func() {
		_ = x.grpc.Serve(lis)
	}()
	return nil
}

func (x *Otel) Cleanup() {
	log.Println("stopping Otel server")
	x.grpc.GracefulStop()
}
