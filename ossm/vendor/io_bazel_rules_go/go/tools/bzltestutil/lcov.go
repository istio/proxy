// Copyright 2022 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package bzltestutil

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"testing/internal/testdeps"
)

// Lock in the COVERAGE_DIR during test setup in case the test uses e.g. os.Clearenv.
var coverageDir = os.Getenv("COVERAGE_DIR")

// ConvertCoverToLcov converts the go coverprofile file coverage.dat.cover to
// the expectedLcov format and stores it in coverage.dat, where it is picked up by
// Bazel.
// The conversion emits line and branch coverage, but not function coverage.
func ConvertCoverToLcov() error {
	inPath := flag.Lookup("test.coverprofile").Value.String()
	in, err := os.Open(inPath)
	if err != nil {
		// This can happen if there are no tests and should not be an error.
		log.Printf("Not collecting coverage: %s has not been created: %s", inPath, err)
		return nil
	}
	defer in.Close()

	if coverageDir == "" {
		log.Printf("Not collecting coverage: COVERAGE_DIR is not set")
		return nil
	}
	// All *.dat files in $COVERAGE_DIR will be merged by Bazel's lcov_merger tool.
	out, err := os.CreateTemp(coverageDir, "go_coverage.*.dat")
	if err != nil {
		return err
	}
	defer out.Close()

	return convertCoverToLcov(in, out)
}

var _coverLinePattern = regexp.MustCompile(`^(?P<path>.+):(?P<startLine>\d+)\.(?P<startColumn>\d+),(?P<endLine>\d+)\.(?P<endColumn>\d+) (?P<numStmt>\d+) (?P<count>\d+)$`)

const (
	_pathIdx      = 1
	_startLineIdx = 2
	_endLineIdx   = 4
	_countIdx     = 7
)

func convertCoverToLcov(coverReader io.Reader, lcovWriter io.Writer) error {
	cover := bufio.NewScanner(coverReader)
	lcov := bufio.NewWriter(lcovWriter)
	defer lcov.Flush()
	currentPath := ""
	var lineCounts map[uint32]uint32
	for cover.Scan() {
		l := cover.Text()
		m := _coverLinePattern.FindStringSubmatch(l)
		if m == nil {
			if strings.HasPrefix(l, "mode: ") {
				continue
			}
			return fmt.Errorf("invalid go cover line: %s", l)
		}

		if m[_pathIdx] != currentPath {
			if currentPath != "" {
				if err := emitLcovLines(lcov, currentPath, lineCounts); err != nil {
					return err
				}
			}
			currentPath = m[_pathIdx]
			lineCounts = make(map[uint32]uint32)
		}

		startLine, err := strconv.ParseUint(m[_startLineIdx], 10, 32)
		if err != nil {
			return err
		}
		endLine, err := strconv.ParseUint(m[_endLineIdx], 10, 32)
		if err != nil {
			return err
		}
		count, err := strconv.ParseUint(m[_countIdx], 10, 32)
		if err != nil {
			return err
		}
		for line := uint32(startLine); line <= uint32(endLine); line++ {
			prevCount, ok := lineCounts[line]
			if !ok || uint32(count) > prevCount {
				lineCounts[line] = uint32(count)
			}
		}
	}
	if currentPath != "" {
		if err := emitLcovLines(lcov, currentPath, lineCounts); err != nil {
			return err
		}
	}
	return nil
}

func emitLcovLines(lcov io.StringWriter, path string, lineCounts map[uint32]uint32) error {
	_, err := lcov.WriteString(fmt.Sprintf("SF:%s\n", path))
	if err != nil {
		return err
	}

	// Emit the coverage counters for the individual source lines.
	sortedLines := make([]uint32, 0, len(lineCounts))
	for line := range lineCounts {
		sortedLines = append(sortedLines, line)
	}
	sort.Slice(sortedLines, func(i, j int) bool { return sortedLines[i] < sortedLines[j] })
	numCovered := 0
	for _, line := range sortedLines {
		count := lineCounts[line]
		if count > 0 {
			numCovered++
		}
		_, err := lcov.WriteString(fmt.Sprintf("DA:%d,%d\n", line, count))
		if err != nil {
			return err
		}
	}
	// Emit a summary containing the number of all/covered lines and end the info for the current source file.
	_, err = lcov.WriteString(fmt.Sprintf("LH:%d\nLF:%d\nend_of_record\n", numCovered, len(sortedLines)))
	if err != nil {
		return err
	}
	return nil
}

// LcovTestDeps is a patched version of testdeps.TestDeps that allows to
// hook into the SetPanicOnExit0 call happening right before testing.M.Run
// returns.
// This trick relies on the testDeps interface defined in this package being
// identical to the actual testing.testDeps interface, which differs between
// major versions of Go.
type LcovTestDeps struct {
	testdeps.TestDeps
	OriginalPanicOnExit bool
}

// SetPanicOnExit0 is called with true by m.Run() before running all tests,
// and with false right before returning -- after writing all coverage
// profiles.
// https://cs.opensource.google/go/go/+/refs/tags/go1.18.1:src/testing/testing.go;l=1921-1931;drc=refs%2Ftags%2Fgo1.18.1
//
// This gives us a good place to intercept the os.Exit(m.Run()) with coverage
// data already available.
func (ltd LcovTestDeps) SetPanicOnExit0(panicOnExit bool) {
	if !panicOnExit {
		lcovAtExitHook()
	}
	ltd.TestDeps.SetPanicOnExit0(ltd.OriginalPanicOnExit)
}

func lcovAtExitHook() {
	if err := ConvertCoverToLcov(); err != nil {
		log.Printf("Failed to collect coverage: %s", err)
		os.Exit(TestWrapperAbnormalExit)
	}
}
