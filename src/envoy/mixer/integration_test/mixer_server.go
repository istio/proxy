// Copyright 2017 Istio Authors
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

package test

import (
	"fmt"
	"io"
	"log"
	"net"

	"github.com/golang/protobuf/proto"
	rpc "github.com/googleapis/googleapis/google/rpc"
	"google.golang.org/grpc"
	mixerpb "istio.io/api/mixer/v1"
)

type Handler struct {
	ctx      *Context
	count    int
	r_status []rpc.Status
}

func newHandler() *Handler {
	return &Handler{
		ctx:      NewContext(),
		count:    0,
		r_status: nil,
	}
}

func (h *Handler) run(attrs *mixerpb.Attributes) *rpc.Status {
	h.ctx.Update(attrs)
	o := &rpc.Status{}
	if h.r_status != nil {
		o = &h.r_status[h.count%len(h.r_status)]
	}
	h.count++
	return o
}

func (h *Handler) check(
	request *mixerpb.CheckRequest, response *mixerpb.CheckResponse) {
	response.RequestIndex = request.RequestIndex
	response.Result = h.run(request.AttributeUpdate)
}

func (h *Handler) report(
	request *mixerpb.ReportRequest, response *mixerpb.ReportResponse) {
	response.RequestIndex = request.RequestIndex
	response.Result = h.run(request.AttributeUpdate)
}

func (h *Handler) quota(
	request *mixerpb.QuotaRequest, response *mixerpb.QuotaResponse) {
	response.RequestIndex = request.RequestIndex
	response.Result = h.run(request.AttributeUpdate)
}

type MixerServer struct {
	lis    net.Listener
	gs     *grpc.Server
	check  *Handler
	report *Handler
	quota  *Handler
}

type handlerFunc func(request proto.Message, response proto.Message)

func (s *MixerServer) streamLoop(stream grpc.ServerStream,
	request proto.Message, response proto.Message, handler handlerFunc) error {
	for {
		// get a single message
		if err := stream.RecvMsg(request); err == io.EOF {
			return nil
		} else if err != nil {
			log.Printf("Stream error %s", err)
			return err
		}

		handler(request, response)

		// produce the response
		if err := stream.SendMsg(response); err != nil {
			return err
		}

		// reset everything to 0
		request.Reset()
		response.Reset()
	}
}

func (s *MixerServer) Check(stream mixerpb.Mixer_CheckServer) error {
	return s.streamLoop(stream,
		new(mixerpb.CheckRequest),
		new(mixerpb.CheckResponse),
		func(request proto.Message, response proto.Message) {
			s.check.check(request.(*mixerpb.CheckRequest),
				response.(*mixerpb.CheckResponse))
		})
}

func (s *MixerServer) Report(stream mixerpb.Mixer_ReportServer) error {
	return s.streamLoop(stream,
		new(mixerpb.ReportRequest),
		new(mixerpb.ReportResponse),
		func(request proto.Message, response proto.Message) {
			s.report.report(request.(*mixerpb.ReportRequest),
				response.(*mixerpb.ReportResponse))
		})
}

func (s *MixerServer) Quota(stream mixerpb.Mixer_QuotaServer) error {
	return s.streamLoop(stream,
		new(mixerpb.QuotaRequest),
		new(mixerpb.QuotaResponse),
		func(request proto.Message, response proto.Message) {
			s.quota.quota(request.(*mixerpb.QuotaRequest),
				response.(*mixerpb.QuotaResponse))
		})
}

func NewMixerServer(port uint16) (*MixerServer, error) {
	log.Printf("Mixer server listening on port %v\n", port)
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
		return nil, err
	}

	var opts []grpc.ServerOption
	opts = append(opts, grpc.MaxConcurrentStreams(32))
	opts = append(opts, grpc.MaxMsgSize(1024*1024))
	gs := grpc.NewServer(opts...)

	s := &MixerServer{
		lis:    lis,
		gs:     gs,
		check:  newHandler(),
		report: newHandler(),
		quota:  newHandler(),
	}
	mixerpb.RegisterMixerServer(gs, s)
	return s, nil
}

func (s *MixerServer) Start() {
	go func() {
		_ = s.gs.Serve(s.lis)
		log.Printf("Mixer server exited\n")
	}()
}

func (s *MixerServer) Stop() {
	log.Printf("Stop Mixer server\n")
	s.gs.Stop()
	log.Printf("Stop Mixer server  -- Done\n")
}
