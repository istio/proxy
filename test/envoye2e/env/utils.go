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
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func GetBazelBin() (string, error) {
	// Get bazel args if any
	buildArgs := os.Getenv("BAZEL_BUILD_ARGS")

	// Note: `bazel info bazel-bin` returns incorrect path to a binary (always fastbuild, not opt or dbg)
	// Instead we rely on symbolic link src/envoy/envoy in the workspace
	args := []string{"info", "workspace"}
	if buildArgs != "" {
		args = append(args, strings.Split(buildArgs, " ")...)
	}
	workspace, err := exec.Command("bazel", args...).Output()
	if err != nil {
		return "", err
	}
	return filepath.Join(strings.TrimSuffix(string(workspace), "\n"), "bazel-bin/"), nil
}

func GetBazelBinOrDie() string {
	bin, err := GetBazelBin()
	if err != nil {
		panic(err)
	}
	return bin
}

func GetDefaultEnvoyBin() (string, error) {
	bin, err := GetBazelBin()
	if err != nil {
		return "", err
	}
	return filepath.Join(bin, "src/envoy/"), nil
}

func GetDefaultEnvoyBinOrDie() string {
	return filepath.Join(GetBazelBinOrDie(), "src/envoy/")
}

func SkipTSanASan(t *testing.T) {
	if os.Getenv("TSAN") != "" || os.Getenv("ASAN") != "" {
		t.Skip("https://github.com/istio/istio/issues/21273")
	}
}

func SkipWasm(t *testing.T, runtime string) {
	if os.Getenv("WASM") != "" {
		if runtime != "envoy.wasm.runtime.v8" {
			t.Skip("Skip test since runtime is not v8")
		}
	} else if runtime == "envoy.wasm.runtime.v8" {
		t.Skip("Skip v8 runtime test since wasm module is not generated")
	}
}

func IsTSanASan() bool {
	return os.Getenv("TSAN") != "" || os.Getenv("ASAN") != ""
}
