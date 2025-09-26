//go:build darwin

package common

import (
	"os"

	"golang.org/x/sys/unix"
)

// https://keith.github.io/xcode-man-pages/clonefile.2.html
func cloneFile(src, dst string) (supported bool, err error) {
	supported = true
	if err = unix.Clonefile(src, dst, 0); err != nil {
		err = &os.LinkError{
			Op:  "clonefile",
			Old: src,
			New: dst,
			Err: err,
		}
	}
	return
}
