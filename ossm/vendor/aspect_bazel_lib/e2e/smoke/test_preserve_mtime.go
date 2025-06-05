package smoketest

import (
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/bazelbuild/rules_go/go/tools/bazel"
)

type runfilePath struct {
	// runfileDir is the directory that was provided as the bazel data dep.
	runfileDir string
	// subPaths to look for inside of the bazel tracked directory.
	subPaths []string
}

func mtime(path string) (time.Time, error) {
	info, err := os.Stat(path)
	if err != nil {
		return time.Time{}, err
	}
	return info.ModTime(), nil
}

func (r runfilePath) osPath() (string, error) {
	dirPath, err := bazel.Runfile(r.runfileDir)
	if err != nil {
		return "", err
	}
	parts := append([]string{dirPath}, r.subPaths...)
	return filepath.Join(parts...), nil
}

func TestPreserveMTime(t *testing.T) {
	cases := map[string]struct {
		original runfilePath
		copied   runfilePath
	}{
		"copy_directory": {
			original: runfilePath{
				runfileDir: "d",
				subPaths:   []string{"1"},
			},
			copied: runfilePath{
				runfileDir: "copy_directory_mtime_out",
				subPaths:   []string{"1"},
			},
		},
		"copy_to_directory": {
			original: runfilePath{
				runfileDir: "d",
				subPaths:   []string{"1"},
			},
			copied: runfilePath{
				runfileDir: "copy_to_directory_mtime_out",
				subPaths:   []string{"d", "1"},
			},
		},
	}

	for name, test := range cases {
		t.Run(name, func(t *testing.T) {
			originalPath, err := test.original.osPath()
			if err != nil {
				t.Fatal(err.Error())
			}
			originalMTime, err := mtime(originalPath)
			if err != nil {
				t.Fatal(err.Error())
			}

			copiedPath, err := test.copied.osPath()
			if err != nil {
				t.Fatal(err.Error())
			}
			copiedMTime, err := mtime(copiedPath)
			if err != nil {
				t.Fatal(err.Error())
			}

			if originalMTime != copiedMTime {
				t.Fatalf(`Modify times do not match for %s and %s:
  Original modify time: %s
  Copied modify time:   %s`, originalPath, copiedPath, originalMTime, copiedMTime)
			}
		})
	}
}
