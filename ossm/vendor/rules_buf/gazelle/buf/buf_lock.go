// Copyright 2021-2023 Buf Technologies, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package buf

// bufLock is subset of the `buf.lock` representation copied from bufbuild/buf
//
// Must be kept in sync with: bufbuild/buf/private/bufpkg/buflock.ExternalConfigV1
type bufLock struct {
	Version string `yaml:"version,omitempty" json:"version,omitempty"`
	Deps    []struct {
		Remote     string `yaml:"remote,omitempty" json:"remote,omitempty"`
		Owner      string `yaml:"owner,omitempty" json:"owner,omitempty"`
		Repository string `yaml:"repository,omitempty" json:"repository,omitempty"`
		Commit     string `yaml:"commit,omitempty" json:"commit,omitempty"`
		Name       string `json:"name,omitempty" yaml:"name,omitempty"`
	} `yaml:"deps,omitempty" json:"deps,omitempty"`
}
