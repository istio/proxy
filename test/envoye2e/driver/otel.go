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

	"github.com/golang/protobuf/proto"
	collogspb "go.opentelemetry.io/proto/otlp/collector/logs/v1"
	colmetricspb "go.opentelemetry.io/proto/otlp/collector/metrics/v1"
	"google.golang.org/grpc"
)

type OtelLogs struct {
	collogspb.UnimplementedLogsServiceServer
}

type OtelMetrics struct {
	colmetricspb.UnimplementedMetricsServiceServer
}

type Otel struct {
	OtelLogs
	OtelMetrics
	grpc *grpc.Server
	Port uint16
}

func (x *OtelLogs) Export(ctx context.Context, req *collogspb.ExportLogsServiceRequest) (*collogspb.ExportLogsServiceResponse, error) {
	log.Printf("log=%s\n", proto.MarshalTextString(req))
	return &collogspb.ExportLogsServiceResponse{}, nil
}

func (x *OtelMetrics) Export(ctx context.Context, req *colmetricspb.ExportMetricsServiceRequest) (*colmetricspb.ExportMetricsServiceResponse, error) {
	//log.Printf("metric=%s\n", proto.MarshalTextString(req))
	return &colmetricspb.ExportMetricsServiceResponse{}, nil
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
	go func() {
		_ = x.grpc.Serve(lis)
	}()
	return nil
}

func (x *Otel) Cleanup() {
	log.Println("stopping Otel server")
	x.grpc.GracefulStop()
}
