/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package main

import (
	"fmt"
	"html/template"
	"os"
	"path"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestReadEnvoyExtensionsFromUpstream(t *testing.T) {
	commitid := "e368c3e5e6dcbcb893364dda99b71ff9d68f3158"
	got, err := readEnvoyExtensions("envoyproxy", "envoy", commitid)
	assert.NoError(t, err)

	expected := readOut(commitid)
	assert.Equal(t, expected, got)

	bzl, err := eval(ExtensionsOptions{
		EnvoyExtensions: template.HTML(got),
	})
	assert.NoError(t, err)
	expectedBzl := readBzl(commitid)
	assert.Equal(t, expectedBzl, string(bzl))
}

func readOut(commitid string) string {
	b, _ := os.ReadFile(path.Join("testdata", fmt.Sprintf("%s.out", commitid)))
	return string(b)
}

func readBzl(commitid string) string {
	b, _ := os.ReadFile(path.Join("testdata", fmt.Sprintf("%s.bzl.out", commitid)))
	return string(b)
}
