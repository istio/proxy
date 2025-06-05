/* Copyright 2016 The Bazel Authors. All rights reserved.

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

package main

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"

	"github.com/bazelbuild/bazel-gazelle/config"
	"github.com/bazelbuild/bazel-gazelle/rule"
)

func fixFile(c *config.Config, f *rule.File) error {
	newContent := f.Format()
	if bytes.Equal(f.Content, newContent) {
		return nil
	}
	outPath := findOutputPath(c, f)
	if err := os.MkdirAll(filepath.Dir(outPath), 0o777); err != nil {
		return err
	}
	if err := os.WriteFile(outPath, newContent, 0o666); err != nil {
		return err
	}
	f.Content = newContent
	if getUpdateConfig(c).print0 {
		fmt.Printf("%s\x00", outPath)
	}
	return nil
}
