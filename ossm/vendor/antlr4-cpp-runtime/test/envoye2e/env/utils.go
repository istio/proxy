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

func GetBazelWorkspace() (string, error) {
	// Get bazel args if any
	buildArgs := os.Getenv("BAZEL_BUILD_ARGS")

	// Note: `bazel info bazel-bin` returns incorrect path to a binary (always fastbuild, not opt or dbg)
	// Instead we rely on symbolic link envoy in the workspace
	args := []string{"info", "workspace"}
	if buildArgs != "" {
		args = append(args, strings.Split(buildArgs, " ")...)
	}
	workspace, err := exec.Command("bazel", args...).Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSuffix(string(workspace), "\n"), nil
}

func GetBazelWorkspaceOrDie() string {
	bin, err := GetBazelWorkspace()
	if err != nil {
		panic(err)
	}
	return bin
}

func GetBazelBin() (string, error) {
	workspace, err := GetBazelWorkspace()
	if err != nil {
		return "", err
	}
	return filepath.Join(workspace, "bazel-bin/"), nil
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
	return bin, nil
}

func GetDefaultEnvoyBinOrDie() string {
	return GetBazelBinOrDie()
}

func SkipTSanASan(t *testing.T) {
	if os.Getenv("TSAN") != "" || os.Getenv("ASAN") != "" {
		t.Skip("https://github.com/istio/istio/issues/21273")
	}
}

func SkipTSan(t *testing.T) {
	if os.Getenv("TSAN") != "" {
		t.Skip("https://github.com/istio/istio/issues/21273")
	}
}

func IsTSanASan() bool {
	return os.Getenv("TSAN") != "" || os.Getenv("ASAN") != ""
}
