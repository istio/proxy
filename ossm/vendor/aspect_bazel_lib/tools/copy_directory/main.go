package main

import (
	"fmt"
	"io/fs"
	"log"
	"os"
	"path/filepath"
	"sync"

	"github.com/bazel-contrib/bazel-lib/tools/common"
)

type pathSet map[string]bool

var (
	srcPaths      = pathSet{}
	hardlink      = false
	verbose       = false
	preserveMTime = false
)

type walker struct {
	queue chan<- common.CopyOpts
}

func (w *walker) copyDir(src string, dst string) error {
	// filepath.WalkDir walks the file tree rooted at root, calling fn for each file or directory in
	// the tree, including root. See https://pkg.go.dev/path/filepath#WalkDir for more info.
	return filepath.WalkDir(src, func(p string, dirEntry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		r, err := common.FileRel(src, p)
		if err != nil {
			return err
		}

		d := filepath.Join(dst, r)

		if dirEntry.IsDir() {
			srcPaths[src] = true
			return os.MkdirAll(d, os.ModePerm)
		}

		info, err := dirEntry.Info()
		if err != nil {
			return err
		}

		if info.Mode()&os.ModeSymlink == os.ModeSymlink {
			// symlink to directories are intentionally never followed by filepath.Walk to avoid infinite recursion
			linkPath, err := common.Realpath(p)
			if err != nil {
				if os.IsNotExist(err) {
					return fmt.Errorf("failed to get realpath of dangling symlink %s: %w", p, err)
				}
				return fmt.Errorf("failed to get realpath of %s: %w", p, err)
			}
			if srcPaths[linkPath] {
				// recursive symlink; silently ignore
				return nil
			}
			stat, err := os.Stat(linkPath)
			if err != nil {
				return fmt.Errorf("failed to stat file %s pointed to by symlink %s: %w", linkPath, p, err)
			}
			if stat.IsDir() {
				// symlink points to a directory
				return w.copyDir(linkPath, d)
			} else {
				// symlink points to a regular file
				w.queue <- common.NewCopyOpts(linkPath, d, stat, hardlink, verbose, preserveMTime)
				return nil
			}
		}

		// a regular file
		w.queue <- common.NewCopyOpts(p, d, info, hardlink, verbose, preserveMTime)
		return nil
	})
}

func main() {
	args := os.Args[1:]

	if len(args) < 2 {
		fmt.Println("Usage: copy_directory src dst [--hardlink] [--verbose] [--preserve-mtime]")
		os.Exit(1)
	}

	src := args[0]
	dst := args[1]

	if len(args) > 2 {
		for _, a := range os.Args[2:] {
			if a == "--hardlink" {
				hardlink = true
			} else if a == "--verbose" {
				verbose = true
			} else if a == "--preserve-mtime" {
				preserveMTime = true
			}
		}
	}

	queue := make(chan common.CopyOpts, 100)
	var wg sync.WaitGroup

	const numWorkers = 10
	wg.Add(numWorkers)
	for i := 0; i < numWorkers; i++ {
		go common.NewCopyWorker(queue).Run(&wg)
	}

	walker := &walker{queue}
	if err := walker.copyDir(src, dst); err != nil {
		log.Fatal(err)
	}
	close(queue)
	wg.Wait()
}
