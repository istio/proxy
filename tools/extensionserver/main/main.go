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

package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"net"

	"google.golang.org/grpc"

	discoveryservice "github.com/envoyproxy/go-control-plane/envoy/service/discovery/v3"
	extensionservice "github.com/envoyproxy/go-control-plane/envoy/service/extension/v3"
	"istio.io/proxy/tools/extensionserver"
)

const (
	grpcMaxConcurrentStreams = 100000
)

var (
	port   uint
	server *extensionserver.ExtensionServer
)

func init() {
	flag.UintVar(&port, "port", 8080, "xDS management server port")
}

func main() {
	flag.Parse()

	grpcServer := grpc.NewServer(grpc.MaxConcurrentStreams(grpcMaxConcurrentStreams))
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		log.Fatal(err)
	}
	server := extensionserver.New(context.Background())
	discoveryservice.RegisterAggregatedDiscoveryServiceServer(grpcServer, server)
	extensionservice.RegisterExtensionConfigDiscoveryServiceServer(grpcServer, server)

	log.Printf("management server listening on %d\n", port)
	if err = grpcServer.Serve(lis); err != nil {
		log.Println(err)
	}
	_ = server
}
