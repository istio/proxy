/* Copyright 2020 The Bazel Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

// This test file was first seen on:
// https://github.com/bazelbuild/bazel-skylib/blob/f80bc733d4b9f83d427ce3442be2e07427b2cc8d/gazelle/bzl/BUILD.
// It was modified for the needs of this extension.

package python_test

import (
	"bytes"
	"context"
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/bazelbuild/bazel-gazelle/testtools"
	"github.com/bazelbuild/rules_go/go/tools/bazel"
	"github.com/ghodss/yaml"
)

const (
	extensionDir      = "python" + string(os.PathSeparator)
	testDataPath      = extensionDir + "testdata" + string(os.PathSeparator)
	gazelleBinaryName = "gazelle_binary"
)

func TestGazelleBinary(t *testing.T) {
	gazellePath := mustFindGazelle()
	tests := map[string][]bazel.RunfileEntry{}

	runfiles, err := bazel.ListRunfiles()
	if err != nil {
		t.Fatalf("bazel.ListRunfiles() error: %v", err)
	}
	for _, f := range runfiles {
		if strings.HasPrefix(f.ShortPath, testDataPath) {
			relativePath := strings.TrimPrefix(f.ShortPath, testDataPath)
			parts := strings.SplitN(relativePath, string(os.PathSeparator), 2)
			if len(parts) < 2 {
				// This file is not a part of a testcase since it must be in a dir that
				// is the test case and then have a path inside of that.
				continue
			}

			tests[parts[0]] = append(tests[parts[0]], f)
		}
	}
	if len(tests) == 0 {
		t.Fatal("no tests found")
	}
	for testName, files := range tests {
		testPath(t, gazellePath, testName, files)
	}
}

func testPath(t *testing.T, gazellePath, name string, files []bazel.RunfileEntry) {
	t.Run(name, func(t *testing.T) {
		t.Parallel()
		var inputs, goldens []testtools.FileSpec

		var config *testYAML
		for _, f := range files {
			path := f.Path
			trim := filepath.Join(testDataPath, name) + string(os.PathSeparator)
			shortPath := strings.TrimPrefix(f.ShortPath, trim)
			info, err := os.Stat(path)
			if err != nil {
				t.Fatalf("os.Stat(%q) error: %v", path, err)
			}

			if info.IsDir() {
				continue
			}

			content, err := os.ReadFile(path)
			if err != nil {
				t.Errorf("os.ReadFile(%q) error: %v", path, err)
			}

			if filepath.Base(shortPath) == "test.yaml" {
				if config != nil {
					t.Fatal("only 1 test.yaml is supported")
				}
				config = new(testYAML)
				if err := yaml.Unmarshal(content, config); err != nil {
					t.Fatal(err)
				}
			}

			if strings.HasSuffix(shortPath, ".in") {
				inputs = append(inputs, testtools.FileSpec{
					Path:    filepath.Join(name, strings.TrimSuffix(shortPath, ".in")),
					Content: string(content),
				})
				continue
			}

			if strings.HasSuffix(shortPath, ".out") {
				goldens = append(goldens, testtools.FileSpec{
					Path:    filepath.Join(name, strings.TrimSuffix(shortPath, ".out")),
					Content: string(content),
				})
				continue
			}

			inputs = append(inputs, testtools.FileSpec{
				Path:    filepath.Join(name, shortPath),
				Content: string(content),
			})
			goldens = append(goldens, testtools.FileSpec{
				Path:    filepath.Join(name, shortPath),
				Content: string(content),
			})
		}

		testdataDir, cleanup := testtools.CreateFiles(t, inputs)
		t.Cleanup(cleanup)
		t.Cleanup(func() {
			if !t.Failed() {
				return
			}

			filepath.Walk(testdataDir, func(path string, info os.FileInfo, err error) error {
				if err != nil {
					return err
				}
				t.Logf("%q exists", strings.TrimPrefix(path, testdataDir))
				return nil
			})
		})

		workspaceRoot := filepath.Join(testdataDir, name)

		args := []string{"-build_file_name=BUILD,BUILD.bazel"}

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		t.Cleanup(cancel)
		cmd := exec.CommandContext(ctx, gazellePath, args...)
		var stdout, stderr bytes.Buffer
		cmd.Stdout = &stdout
		cmd.Stderr = &stderr
		cmd.Dir = workspaceRoot
		if err := cmd.Run(); err != nil {
			var e *exec.ExitError
			if !errors.As(err, &e) {
				t.Fatal(err)
			}
		}

		actualExitCode := cmd.ProcessState.ExitCode()
		if config.Expect.ExitCode != actualExitCode {
			t.Errorf("expected gazelle exit code: %d\ngot: %d",
				config.Expect.ExitCode, actualExitCode)
		}
		actualStdout := stdout.String()
		if strings.TrimSpace(config.Expect.Stdout) != strings.TrimSpace(actualStdout) {
			t.Errorf("expected gazelle stdout: %s\ngot: %s",
				config.Expect.Stdout, actualStdout)
		}
		actualStderr := stderr.String()
		if strings.TrimSpace(config.Expect.Stderr) != strings.TrimSpace(actualStderr) {
			t.Errorf("expected gazelle stderr: %s\ngot: %s",
				config.Expect.Stderr, actualStderr)
		}
		if t.Failed() {
			t.FailNow()
		}

		testtools.CheckFiles(t, testdataDir, goldens)
	})
}

func mustFindGazelle() string {
	gazellePath, ok := bazel.FindBinary(extensionDir, gazelleBinaryName)
	if !ok {
		panic("could not find gazelle binary")
	}
	return gazellePath
}

type testYAML struct {
	Expect struct {
		ExitCode int    `json:"exit_code"`
		Stdout   string `json:"stdout"`
		Stderr   string `json:"stderr"`
	} `json:"expect"`
}
