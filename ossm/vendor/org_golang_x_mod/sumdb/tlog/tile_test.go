// Copyright 2019 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package tlog

import (
	"testing"
)

// FuzzParseTilePath tests that ParseTilePath never crashes
func FuzzParseTilePath(f *testing.F) {
	f.Add("tile/4/0/001")
	f.Add("tile/4/0/001.p/5")
	f.Add("tile/3/5/x123/x456/078")
	f.Add("tile/3/5/x123/x456/078.p/2")
	f.Add("tile/1/0/x003/x057/500")
	f.Add("tile/3/5/123/456/078")
	f.Add("tile/3/-1/123/456/078")
	f.Add("tile/1/data/x003/x057/500")
	f.Fuzz(func(t *testing.T, path string) {
		ParseTilePath(path)
	})
}
