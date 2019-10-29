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

package env

import (
	"fmt"
	"go/build"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
)

func GetDefaultIstioOut() string {
	return fmt.Sprintf("%s/out/%s_%s", build.Default.GOPATH, runtime.GOOS, runtime.GOARCH)
}

func GetDefaultEnvoyBin() string {
	// Note: `bazel info bazel-bin` returns incorrect path to a binary (always fastbuild, not opt or dbg)
	// Instead we rely on symbolic link src/envoy/envoy in the workspace
	workspace, _ := exec.Command("bazel", "info", "workspace").Output()
	return filepath.Join(strings.TrimSuffix(string(workspace), "\n"), "bazel-bin/src/envoy/")
}
