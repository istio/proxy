/* Copyright 2018 The Bazel Authors. All rights reserved.

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

package go_path

import (
	"archive/zip"
	"flag"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"github.com/bazelbuild/rules_go/go/tools/bazel"
)

var copyPath, embedPath, embedNoSrcsPath, archivePath, nodataPath, notransitivePath string

var defaultMode = runtime.GOOS + "_" + runtime.GOARCH

var files = []string{
	"extra.txt",
	"src/",
	"-src/example.com/repo/cmd/bin/bin",
	"-src/testmain/testmain.go",
	"src/example.com/repo/cmd/bin/bin.go",
	"src/example.com/repo/pkg/lib/lib.go",
	"src/example.com/repo/pkg/lib/embedded_src.txt",
	"src/example.com/repo/pkg/lib/template/index.html.tmpl",
	"src/example.com/repo/pkg/lib/embed_test.go",
	"src/example.com/repo/pkg/lib/internal_test.go",
	"src/example.com/repo/pkg/lib/external_test.go",
	"-src/example.com/repo/pkg/lib_test/embed_test.go",
	"src/example.com/repo/pkg/lib/data.txt",
	"src/example.com/repo/pkg/lib/testdata/testdata.txt",
	"src/example.com/repo/vendor/example.com/repo2/vendored.go",
	"pkg/" + defaultMode + "/example.com/repo/cmd/bin.a",
	"pkg/" + defaultMode + "/example.com/repo/pkg/lib.a",
	"pkg/" + defaultMode + "/example.com/repo/vendor/example.com/repo2.a",
	"pkg/plan9_arm/example.com/repo/cmd/bin.a",
}

func TestMain(m *testing.M) {
	flag.StringVar(&copyPath, "copy_path", "", "path to copied go_path")
	flag.StringVar(&archivePath, "archive_path", "", "path to archive go_path")
	flag.StringVar(&nodataPath, "nodata_path", "", "path to go_path without data")
	flag.StringVar(&embedPath, "embed_path", "", "path to go_path with embedsrcs")
	flag.StringVar(&embedNoSrcsPath, "embed_no_srcs_path", "", "path to go_path with embedsrcs")
	flag.StringVar(&notransitivePath, "notransitive_path", "", "path to go_path without transitive dependencies")
	flag.Parse()
	os.Exit(m.Run())
}

func TestCopyPath(t *testing.T) {
	if copyPath == "" {
		t.Fatal("-copy_path not set")
	}
	checkPath(t, copyPath, files)
}

func TestEmbedPath(t *testing.T) {
	if embedPath == "" {
		t.Fatal("-embed_path not set")
	}
	files := []string{
		"src/lib/embed_test.go",
		"src/lib/embedded_src.txt",
		"src/lib/generated_embeded.go",
	}
	checkPath(t, embedPath, files)
}

func TestEmbedNoSrcsPath(t *testing.T) {
	if embedNoSrcsPath == "" {
		t.Fatal("-embed_no_srcs_path not set")
	}
	files := []string{
		"src/lib/embedded_src.txt",
		"src/lib/generated_embeded_no_srcs.go",
	}
	checkPath(t, embedNoSrcsPath, files)
}

func TestArchivePath(t *testing.T) {
	if archivePath == "" {
		t.Fatal("-archive_path not set")
	}
	dir, err := ioutil.TempDir(os.Getenv("TEST_TEMPDIR"), "TestArchivePath")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(dir)

	path, err := bazel.Runfile(archivePath)
	if err != nil {
		t.Fatalf("Could not find runfile %s: %q", archivePath, err)
	}

	z, err := zip.OpenReader(path)
	if err != nil {
		t.Fatalf("error opening zip: %v", err)
	}
	defer z.Close()
	for _, f := range z.File {
		r, err := f.Open()
		if err != nil {
			t.Fatalf("error reading file %s: %v", f.Name, err)
		}
		dstPath := filepath.Join(dir, filepath.FromSlash(f.Name))
		if err := os.MkdirAll(filepath.Dir(dstPath), 0777); err != nil {
			t.Fatalf("error creating directory %s: %v", filepath.Dir(dstPath), err)
		}
		w, err := os.Create(dstPath)
		if err != nil {
			t.Fatalf("error creating file %s: %v", dstPath, err)
		}
		if _, err := io.Copy(w, r); err != nil {
			w.Close()
			t.Fatalf("error writing file %s: %v", dstPath, err)
		}
		if err := w.Close(); err != nil {
			t.Fatalf("error closing file %s: %v", dstPath, err)
		}
	}

	checkPath(t, dir, files)
}

func TestNoDataPath(t *testing.T) {
	if nodataPath == "" {
		t.Fatal("-nodata_path not set")
	}
	files := []string{
		"extra.txt",
		"src/example.com/repo/pkg/lib/lib.go",
		"-src/example.com/repo/pkg/lib/data.txt",
	}
	checkPath(t, nodataPath, files)
}

func TestNoTransitivePath(t *testing.T) {
	if notransitivePath == "" {
		t.Fatal("-notransitive_path not set")
	}
	files := []string{
		"-src/example.com/repo/pkg/lib/transitive/transitive.go",
	}
	checkPath(t, notransitivePath, files)
}

// checkPath checks that dir contains a list of files. files is a list of
// slash-separated paths relative to dir. Files that start with "-" should be
// absent. Files that end with "/" should be directories.
func checkPath(t *testing.T, dir string, files []string) {
	if strings.HasPrefix(dir, "external") {
		dir = filepath.Join(os.Getenv("TEST_SRCDIR"), strings.TrimPrefix(dir, "external/"))
	}

	for _, f := range files {
		wantDir := strings.HasSuffix(f, "/")
		wantAbsent := false
		if strings.HasPrefix(f, "-") {
			f = f[1:]
			wantAbsent = true
		}
		path := filepath.Join(dir, filepath.FromSlash(f))
		st, err := os.Stat(path)
		if wantAbsent {
			if err == nil {
				t.Errorf("found %s: should not be present", path)
			} else if !os.IsNotExist(err) {
				t.Error(err)
			}
		} else {
			if err != nil {
				if os.IsNotExist(err) {
					t.Errorf("%s is missing", path)
				} else {
					t.Error(err)
				}
				continue
			}
			if st.IsDir() && !wantDir {
				t.Errorf("%s: got directory; wanted file", path)
			} else if !st.IsDir() && wantDir {
				t.Errorf("%s: got file; wanted directory", path)
			}
		}
	}
}
