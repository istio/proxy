//go:build !darwin

package common

func cloneFile(src, dst string) (supported bool, err error) {
	supported = false
	err = nil
	return
}
