// Copyright Istio Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package extensionserver

import (
	"crypto/sha256"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"time"
)

// Prefetch cache keyed by SHA
// TODO: add expiry
var prefetch = make(map[string][]byte)

var timeout = 5 * time.Second

// Prefetch re-uses the cache is sha256 is specified
func (ext *Extension) Prefetch() error {
	// skip for null
	if ext.Runtime == "null" {
		return nil
	}
	// skip if already available
	if ext.SHA256 != "" {
		if _, prefetched := prefetch[ext.SHA256]; prefetched {
			return nil
		}
	}

	var code []byte
	var err error
	if ext.Path != "" {
		// load code as bytes
		code, err = ioutil.ReadFile(ext.Path)
		if err != nil {
			return err
		}
	} else if ext.URL != "" {
		client := &http.Client{Timeout: timeout}
		resp, err := client.Get(ext.URL)
		if err != nil {
			return err
		}
		code, err = ioutil.ReadAll(resp.Body)
		if err != nil {
			return err
		}
	}

	h := sha256.New()
	if _, err = h.Write(code); err != nil {
		return err
	}
	sha256 := fmt.Sprintf("%x", h.Sum(nil))
	if ext.SHA256 != "" && ext.SHA256 != sha256 {
		return fmt.Errorf("mis-matched SHA256 for %q: got %q, want %q", ext.Name, sha256, ext.SHA256)
	}
	ext.SHA256 = sha256
	prefetch[ext.SHA256] = code
	log.Printf("fetched extension %q, sha256 %q\n", ext.Name, ext.SHA256)
	return nil
}
