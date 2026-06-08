// Copyright 2023 The Bazel Authors. All rights reserved.
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

/*
test.go is a unit test that asserts the Gazelle YAML manifest is up-to-date in
regards to the requirements.txt.

It re-hashes the requirements.txt and compares it to the recorded one in the
existing generated Gazelle manifest.
*/
package test

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/bazel-contrib/rules_python/gazelle/manifest"
	"github.com/bazelbuild/rules_go/go/runfiles"
)

// getResolvedRunfile resolves an environment variable to a runfiles path.
// It handles getting the env var, checking it's set, and resolving it through
// the runfiles mechanism, providing detailed error messages if anything fails.
func getResolvedRunfile(t *testing.T, envVar string) string {
	t.Helper()
	path := os.Getenv(envVar)
	if path == "" {
		t.Fatalf("%s must be set", envVar)
	}
	resolvedPath, err := runfiles.Rlocation(path)
	if err != nil {
		t.Fatalf("failed to resolve runfiles path for %s (%q): %v", envVar, path, err)
	}
	return resolvedPath
}

func TestGazelleManifestIsUpdated(t *testing.T) {
	requirementsPathResolved := getResolvedRunfile(t, "_TEST_REQUIREMENTS")
	manifestPathResolved := getResolvedRunfile(t, "_TEST_MANIFEST")

	manifestFile := new(manifest.File)
	if err := manifestFile.Decode(manifestPathResolved); err != nil {
		t.Fatalf("decoding manifest file: %v", err)
	}

	if manifestFile.Integrity == "" {
		t.Fatal("failed to find the Gazelle manifest file integrity")
	}

	manifestGeneratorHashPath := getResolvedRunfile(t, "_TEST_MANIFEST_GENERATOR_HASH")

	manifestGeneratorHash, err := os.Open(manifestGeneratorHashPath)
	if err != nil {
		t.Fatalf("opening %q: %v", manifestGeneratorHashPath, err)
	}
	defer manifestGeneratorHash.Close()

	requirements, err := os.Open(requirementsPathResolved)
	if err != nil {
		t.Fatalf("opening %q: %v", requirementsPathResolved, err)
	}
	defer requirements.Close()

	valid, err := manifestFile.VerifyIntegrity(manifestGeneratorHash, requirements)
	if err != nil {
		t.Fatalf("verifying integrity: %v", err)
	}
	if !valid {
		manifestRealpath, err := filepath.EvalSymlinks(manifestPathResolved)
		if err != nil {
			t.Fatalf("evaluating symlink %q: %v", manifestPathResolved, err)
		}
		t.Errorf(
			"%q is out-of-date. Follow the update instructions in that file to resolve this",
			manifestRealpath)
	}
}
