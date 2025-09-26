package common

import (
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"sync"
	"time"
)

// From https://opensource.com/article/18/6/copying-files-go
func CopyFile(src string, dst string) error {
	source, err := os.Open(src)
	if err != nil {
		return err
	}
	defer source.Close()

	destination, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer destination.Close()
	_, err = io.Copy(destination, source)
	return err
}

func Copy(opts CopyOpts) {
	if !opts.srcInfo.Mode().IsRegular() {
		log.Fatalf("%s is not a regular file", opts.src)
	}

	opModifier := ""
	const fallback = " (fallback)"

	if opts.hardlink {
		// hardlink this file
		if opts.verbose {
			fmt.Printf("hardlink%s %v => %v\n", opModifier, opts.src, opts.dst)
		}
		err := os.Link(opts.src, opts.dst)
		if err != nil {
			if opts.verbose {
				fmt.Printf("hardlink failed: %v\n", err)
				opModifier = fallback
			}
			// fallback to clonefile or copy
		} else {
			return
		}
	}

	// clone this file
	if opts.verbose {
		fmt.Printf("clonefile%s %v => %v\n", opModifier, opts.src, opts.dst)
	}
	switch supported, err := cloneFile(opts.src, opts.dst); {
	case !supported:
		if opts.verbose {
			fmt.Print("clonefile skipped: not supported by platform\n")
		}
		// fallback to copy
	case supported && err == nil:
		return
	case supported && err != nil:
		if opts.verbose {
			fmt.Printf("clonefile failed: %v\n", err)
			opModifier = fallback
		}
		// fallback to copy
	}

	// copy this file
	if opts.verbose {
		fmt.Printf("copy%s %v => %v\n", opModifier, opts.src, opts.dst)
	}
	err := CopyFile(opts.src, opts.dst)
	if err != nil {
		log.Fatal(err)
	}

	if opts.preserveMTime {
		accessTime := time.Now()
		err := os.Chtimes(opts.dst, accessTime, opts.srcInfo.ModTime())
		if err != nil {
			log.Fatal(err)
		}
	}
}

type CopyWorker struct {
	queue <-chan CopyOpts
}

func NewCopyWorker(queue <-chan CopyOpts) *CopyWorker {
	return &CopyWorker{queue: queue}
}

func (w *CopyWorker) Run(wg *sync.WaitGroup) {
	defer wg.Done()
	for opts := range w.queue {
		Copy(opts)
	}
}

type CopyOpts struct {
	src, dst      string
	srcInfo       fs.FileInfo
	hardlink      bool
	verbose       bool
	preserveMTime bool
}

func NewCopyOpts(src string, dst string, srcInfo fs.FileInfo, hardlink bool, verbose bool, preserveMTime bool) CopyOpts {
	return CopyOpts{src: src, dst: dst, srcInfo: srcInfo, hardlink: hardlink, verbose: verbose, preserveMTime: preserveMTime}
}
