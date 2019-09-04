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
	monitoringpb "google.golang.org/genproto/googleapis/monitoring/v3"
)

// FakeStackdriverServer is a fake stackdriver server which implements all of monitoring v3 service method.
type FakeStackdriverServer struct {
	RcvReq chan *monitoringpb.CreateTimeSeriesRequest
}

// CreateTimeSeries implements CreateTimeSeries method.
func (s *FakeStackdriverServer) CreateTimeSeries(ctx context.Context, req *monitoringpb.CreateTimeSeriesRequest) (*empty.Empty, error) {
	s.RcvReq <- req
	return &empty.Empty{}, nil
}

func newServer() *FakeStackdriverServer {
	return &FakeStackdriverServer{}
}

// NewFakeStackdriver creates a new fake Stackdriver server.
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
