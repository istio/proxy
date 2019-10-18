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
	"istio.io/proxy/test/envoye2e/env"
)

type Backend struct {
	Port uint16

	server *env.HTTPServer
}

var _ Step = &Backend{}

func (b *Backend) Run(p *Params) error {
	var err error
	b.server, err = env.NewHTTPServer(b.Port)
	if err != nil {
		return err
	}

	errCh := b.server.Start()
	return <-errCh
}

func (b *Backend) Cleanup() {
	if b.server != nil {
		b.server.Stop()
	}
}
