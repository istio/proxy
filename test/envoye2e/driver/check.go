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

	"istio.io/proxy/test/envoye2e/env"
)

type Get struct {
	Port uint16
	Body string
}

var _ Step = &Get{}

func (g *Get) Run(_ *Params) error {
	code, body, err := env.HTTPGet(fmt.Sprintf("http://127.0.0.1:%d", g.Port))
	if err != nil {
		return err
	}
	if code != 200 {
		return fmt.Errorf("error code for :%d: %d", g.Port, code)
	}
	if g.Body != "" && g.Body != body {
		return fmt.Errorf("got body %q, want %q", body, g.Body)
	}
	return nil
}
func (g *Get) Cleanup() {}
