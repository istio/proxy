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
	"reflect"
	"time"

	discoveryservice "github.com/envoyproxy/go-control-plane/envoy/service/discovery/v3"
	extensionservice "github.com/envoyproxy/go-control-plane/envoy/service/extension/v3"
	"google.golang.org/grpc"

	"istio.io/proxy/tools/extensionserver"
)

const (
	grpcMaxConcurrentStreams = 100000
)

var (
	port   uint
	dir    string
	sleep  time.Duration
	server *extensionserver.ExtensionServer
	names  = make(map[string]struct{})
)

func init() {
	flag.UintVar(&port, "port", 8080, "xDS management server port")
	flag.StringVar(&dir, "c", "", "Configuration file directory")
	flag.DurationVar(&sleep, "s", 10*time.Second, "Await starting the server for network to be ready")
}

func apply(config *extensionserver.Config) {
	next := make(map[string]struct{})
	for _, ext := range config.Extensions {
		if err := ext.Prefetch(); err != nil {
			log.Printf("error prefetching extension %q: %v\n", ext.Name, err)
			continue
		}
		config, err := extensionserver.Convert(ext)
		if err != nil {
			log.Printf("error loading extension %q: %v\n", ext.Name, err)
			continue
		}
		if err = server.Update(config); err != nil {
			log.Printf("error updating extension %q: %v\n", ext.Name, err)
		}
		next[ext.Name] = struct{}{}
		delete(names, ext.Name)
	}
	for name := range names {
		if err := server.Delete(name); err != nil {
			log.Printf("error deleting extension %q: %v\n", name, err)
		}
	}
	log.Printf("loaded extensions %v, deleted %v\n", reflect.ValueOf(next).MapKeys(), reflect.ValueOf(names).MapKeys())
	names = next
}

func main() {
	flag.Parse()

	log.Printf("waiting %v before start-up...\n", sleep)
	time.Sleep(sleep)

	grpcServer := grpc.NewServer(grpc.MaxConcurrentStreams(grpcMaxConcurrentStreams))
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		log.Fatal(err)
	}
	server = extensionserver.New(context.Background())
	discoveryservice.RegisterAggregatedDiscoveryServiceServer(grpcServer, server)
	extensionservice.RegisterExtensionConfigDiscoveryServiceServer(grpcServer, server)

	log.Printf("watching directory %q\n", dir)
	go func() {
		err := extensionserver.Watch(dir, apply)
		if err != nil {
			log.Println(err)
		}
	}()

	log.Printf("management server listening on %d\n", port)
	if err = grpcServer.Serve(lis); err != nil {
		log.Println(err)
	}
}
