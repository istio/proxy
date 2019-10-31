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
	"fmt"
	"log"
	"net"

	v2 "github.com/envoyproxy/go-control-plane/envoy/api/v2"
	discovery "github.com/envoyproxy/go-control-plane/envoy/service/discovery/v2"
	"github.com/envoyproxy/go-control-plane/pkg/cache"
	"github.com/envoyproxy/go-control-plane/pkg/server"
	"google.golang.org/grpc"
)

// XDS creates an xDS server
type XDS struct {
	grpc *grpc.Server
}

var _ Step = &XDS{}

func (x *XDS) Run(p *Params) error {
	log.Printf("XDS server starting on %d\n", p.XDS)
	x.grpc = grpc.NewServer()
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", p.XDS))
	if err != nil {
		return err
	}

	p.Config = cache.NewSnapshotCache(false, cache.IDHash{}, x)
	xdsServer := server.NewServer(p.Config, nil)
	discovery.RegisterAggregatedDiscoveryServiceServer(x.grpc, xdsServer)

	go func() {
		_ = x.grpc.Serve(lis)
	}()
	return nil
}

func (x *XDS) Cleanup() {
	log.Println("stopping XDS server")
	x.grpc.GracefulStop()
}
func (x *XDS) Infof(format string, args ...interface{}) {
	log.Printf("xds: "+format, args...)
}
func (x *XDS) Errorf(format string, args ...interface{}) {
	log.Printf("xds error: "+format, args...)
}

type Update struct {
	Node      string
	Version   string
	Listeners []string
}

var _ Step = &Update{}

func (u *Update) Run(p *Params) error {
	version, err := p.Fill(u.Version)
	if err != nil {
		return err
	}
	log.Printf("update config for %q with version %q", u.Node, version)
	listeners := make([]cache.Resource, 0, len(u.Listeners))
	for _, listener := range u.Listeners {
		out := &v2.Listener{}
		if err := p.FillYAML(listener, out); err != nil {
			return err
		}
		listeners = append(listeners, out)
	}
	return p.Config.SetSnapshot(u.Node, cache.Snapshot{
		Clusters:  cache.NewResources(version, nil),
		Listeners: cache.NewResources(version, listeners),
	})
}

func (u *Update) Cleanup() {}
