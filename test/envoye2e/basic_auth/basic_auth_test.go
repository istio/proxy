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

package client

import (
	"fmt"
	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
	"os"
	"path/filepath"
	"strconv"
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e/env"
)

func skipWasm(t *testing.T, runtime string) {
	if os.Getenv("WASM") != "" {
		if runtime != "envoy.wasm.runtime.v8" {
			t.Skip("Skip test since runtime is not v8")
		}
	} else if runtime == "envoy.wasm.runtime.v8" {
		t.Skip("Skip v8 runtime test since wasm module is not generated")
	}
}

type capture struct{}

func (capture) Run(p *driver.Params) error {
	prev, err := strconv.Atoi(p.Vars["RequestCount"])
	if err != nil {
		return err
	}
	p.Vars["RequestCount"] = fmt.Sprintf("%d", p.N+prev)
	return nil
}
func (capture) Cleanup() {}

var TestCases = []struct {
	Name           string
	Method         string
	Path           string
	RequestHeaders map[string]string
	ResponseCode   int
}{
	{
		Name:           "CorrectCredentials",
		Method:         "POST",
		Path:           "/api/reviews/pay",
		RequestHeaders: map[string]string{"Authorization": "Basic YWRtaW46YWRtaW4="},
		ResponseCode:   200,
	},
	{
		Name:           "IncorrectCredentials",
		Method:         "GET",
		Path:           "/api/reviews/pay",
		RequestHeaders: map[string]string{"Authorization": "Basic AtRtaW46YWRtaW4="},
		ResponseCode:   401,
	},
	{
		Name:         "MissingCredentials",
		Method:       "GET",
		Path:         "/api/reviews/pay",
		ResponseCode: 401,
	},
	{
		Name:         "NoPathMatch",
		Path:         "/secret",
		ResponseCode: 200,
	},
	{
		Name:         "NoMethodMatch",
		Method:       "DELETE",
		Path:         "/api/reviews/pay",
		ResponseCode: 200,
	},
	{
		Name:         "NoConfigurationCredentialsProvided",
		Method:       "POST",
		Path:         "/api/reviews/pay",
		ResponseCode: 401,
	},
}

var BasicAuthRuntimes = []struct {
	BasicAuthFilterCode string
	WasmRuntime         string
}{
	{
		BasicAuthFilterCode: "inline_string: \"envoy.wasm.basic_auth\"",
		WasmRuntime:         "envoy.wasm.runtime.null",
	},
	{
		BasicAuthFilterCode: "filename: " + filepath.Join(env.GetBazelOptOut(), "extensions/basic_auth.wasm"),
		WasmRuntime:         "envoy.wasm.runtime.v8",
	},
}

func TestBasicAuth(t *testing.T) {
	for _, testCase := range TestCases {
		for _, runtime := range BasicAuthRuntimes {
			t.Run(testCase.Name+"/"+runtime.WasmRuntime, func(t *testing.T) {
				skipWasm(t, runtime.WasmRuntime)
				params := driver.NewTestParams(t, map[string]string{
					"BasicAuthWasmRuntime":  runtime.WasmRuntime,
					"BasicAuthFilterConfig": runtime.BasicAuthFilterCode,
					"Credentials":           driver.LoadTestJSON("testdata/filters/basicauth_configuration_data.txt"),
				}, envoye2e.ProxyE2ETests)
				params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/basicauth.yaml.tmpl")
				if err := (&driver.Scenario{
					[]driver.Step{
						&driver.XDS{},
						&driver.Update{Node: "client", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/client.yaml.tmpl")}},
						&driver.Update{Node: "server", Version: "0", Listeners: []string{params.LoadTestData("testdata/listener/server.yaml.tmpl")}},
						&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
						&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
						&driver.Sleep{Duration: 1 * time.Second},
						&driver.HTTPCall{
							Port:           params.Ports.ClientPort,
							Method:         testCase.Method,
							Path:           testCase.Path,
							RequestHeaders: testCase.RequestHeaders,
							ResponseCode:   testCase.ResponseCode,
						},
					},
				}).Run(params); err != nil {
					t.Fatal(err)
				}
			})
		}
	}
}
