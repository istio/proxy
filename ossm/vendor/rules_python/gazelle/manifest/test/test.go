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

	"github.com/bazelbuild/rules_go/go/runfiles"
	"github.com/bazel-contrib/rules_python/gazelle/manifest"
)

func TestGazelleManifestIsUpdated(t *testing.T) {
	requirementsPath := os.Getenv("_TEST_REQUIREMENTS")
	if requirementsPath == "" {
		t.Fatal("_TEST_REQUIREMENTS must be set")
	}

	manifestPath := os.Getenv("_TEST_MANIFEST")
	if manifestPath == "" {
		t.Fatal("_TEST_MANIFEST must be set")
	}

	manifestFile := new(manifest.File)
	if err := manifestFile.Decode(manifestPath); err != nil {
		t.Fatalf("decoding manifest file: %v", err)
	}

	if manifestFile.Integrity == "" {
		t.Fatal("failed to find the Gazelle manifest file integrity")
	}

	manifestGeneratorHashPath, err := runfiles.Rlocation(
		os.Getenv("_TEST_MANIFEST_GENERATOR_HASH"))
	if err != nil {
		t.Fatalf("failed to resolve runfiles path of manifest: %v", err)
	}

	manifestGeneratorHash, err := os.Open(manifestGeneratorHashPath)
	if err != nil {
		t.Fatalf("opening %q: %v", manifestGeneratorHashPath, err)
	}
	defer manifestGeneratorHash.Close()

	requirements, err := os.Open(requirementsPath)
	if err != nil {
		t.Fatalf("opening %q: %v", requirementsPath, err)
	}
	defer requirements.Close()

	valid, err := manifestFile.VerifyIntegrity(manifestGeneratorHash, requirements)
	if err != nil {
		t.Fatalf("verifying integrity: %v", err)
	}
	if !valid {
		manifestRealpath, err := filepath.EvalSymlinks(manifestPath)
		if err != nil {
			t.Fatalf("evaluating symlink %q: %v", manifestPath, err)
		}
		t.Errorf(
			"%q is out-of-date. Follow the update instructions in that file to resolve this",
			manifestRealpath)
	}
}
