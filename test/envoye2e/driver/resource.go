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
	"io/ioutil"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/golang/protobuf/proto"
)

// Loads resources in the test data directory
// Functions here panic since test artifacts are usually loaded prior to execution,
// so there is no clean-up necessary.

func BazelWorkspace() string {
	workspace, err := exec.Command("bazel", "info", "workspace").Output()
	if err != nil {
		panic(err)
	}
	return strings.TrimSuffix(string(workspace), "\n")
}

// Normalizes test data path
func TestPath(testFileName string) string {
	return filepath.Join(BazelWorkspace(), testFileName)
}

// Loads a test file content
func LoadTestData(testFileName string) string {
	data, err := ioutil.ReadFile(TestPath(testFileName))
	if err != nil {
		panic(err)
	}
	return string(data)
}

// Loads a test file and fills in template variables
func (p *Params) LoadTestData(testFileName string) string {
	data := LoadTestData(testFileName)
	out, err := p.Fill(data)
	if err != nil {
		panic(err)
	}
	return out
}

// Loads a test file as YAML into a proto and fills in template variables
func (p *Params) LoadTestProto(testFileName string, msg proto.Message) {
	data := LoadTestData(testFileName)
	if err := p.FillYAML(data, msg); err != nil {
		panic(err)
	}
}
