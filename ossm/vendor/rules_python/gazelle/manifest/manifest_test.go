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

package manifest_test

import (
	"bytes"
	"log"
	"os"
	"reflect"
	"strings"
	"testing"

	"github.com/bazel-contrib/rules_python/gazelle/manifest"
)

var modulesMapping = manifest.ModulesMapping{
	"arrow": "arrow",
}

const pipDepsRepositoryName = "test_repository_name"

func TestFile(t *testing.T) {
	t.Run("EncodeWithIntegrity", func(t *testing.T) {
		f := manifest.NewFile(&manifest.Manifest{
			ModulesMapping:        modulesMapping,
			PipDepsRepositoryName: pipDepsRepositoryName,
		})
		var b bytes.Buffer
		manifestGeneratorHashFile := strings.NewReader("")
		requirements, err := os.Open("testdata/requirements.txt")
		if err != nil {
			log.Println(err)
			t.FailNow()
		}
		defer requirements.Close()
		if err := f.EncodeWithIntegrity(&b, manifestGeneratorHashFile, requirements); err != nil {
			log.Println(err)
			t.FailNow()
		}
		expected, err := os.ReadFile("testdata/gazelle_python.yaml")
		if err != nil {
			log.Println(err)
			t.FailNow()
		}
		if !bytes.Equal(expected, b.Bytes()) {
			log.Printf("encoded manifest doesn't match expected output: %v\n", b.String())
			t.FailNow()
		}
	})
	t.Run("Decode", func(t *testing.T) {
		f := manifest.NewFile(&manifest.Manifest{})
		if err := f.Decode("testdata/gazelle_python.yaml"); err != nil {
			log.Println(err)
			t.FailNow()
		}
		if !reflect.DeepEqual(modulesMapping, f.Manifest.ModulesMapping) {
			log.Println("decoded modules_mapping doesn't match expected value")
			t.FailNow()
		}
		if f.Manifest.PipDepsRepositoryName != pipDepsRepositoryName {
			log.Println("decoded pip repository name doesn't match expected value")
			t.FailNow()
		}
	})
	t.Run("VerifyIntegrity", func(t *testing.T) {
		f := manifest.NewFile(&manifest.Manifest{})
		if err := f.Decode("testdata/gazelle_python.yaml"); err != nil {
			log.Println(err)
			t.FailNow()
		}
		manifestGeneratorHashFile := strings.NewReader("")
		requirements, err := os.Open("testdata/requirements.txt")
		if err != nil {
			log.Println(err)
			t.FailNow()
		}
		defer requirements.Close()
		valid, err := f.VerifyIntegrity(manifestGeneratorHashFile, requirements)
		if err != nil {
			log.Println(err)
			t.FailNow()
		}
		if !valid {
			log.Println("decoded manifest file is not valid")
			t.FailNow()
		}
	})
}
