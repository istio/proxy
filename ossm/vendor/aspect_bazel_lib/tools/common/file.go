package common

import (
	"os"
	"path"
	"path/filepath"
)

// Same as filepath.Rel except that it normalizes result to forward slashes
// slashes since filepath.Rel will convert to system slashes
func FileRel(basepath, targpath string) (string, error) {
	r, err := filepath.Rel(basepath, targpath)
	if err != nil {
		return "", err
	}

	return filepath.ToSlash(r), nil
}

// github.com/yookoala/realpath has bugs on Windows;
// this function assumes that the path passed in is a symlink
func Realpath(p string) (string, error) {
	t, err := os.Readlink(p)
	if err != nil {
		return "", err
	}

	if !filepath.IsAbs(t) {
		t = path.Join(path.Dir(p), t)
	}

	info, err := os.Lstat(t)
	if err != nil {
		return "", err
	}

	if info.Mode()&os.ModeSymlink == os.ModeSymlink {
		return Realpath(t)
	}

	return t, nil
}
