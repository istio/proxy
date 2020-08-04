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
	"encoding/json"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"reflect"

	"github.com/fsnotify/fsnotify"
	"github.com/ghodss/yaml"
)

// Extension configuration
type Extension struct {
	// Name of the extension
	Name string `json:"name"`
	// Configuration passed as JSON string
	Configuration json.RawMessage `json:"configuration"`
	// Path to the extension code
	Path string `json:"path,omitempty"`
	// VMID (optional)
	VMID string `json:"id,omitempty"`
	// RootID (optional)
	RootID string `json:"root_id,omitempty"`
}

// Config for the extension server
type Config struct {
	// Extensions list
	Extensions []*Extension `json:"extensions"`
}

func (c *Config) merge(that *Config) {
	c.Extensions = append(c.Extensions, that.Extensions...)
}

// Read loads a configuration
func Read(data []byte) (*Config, error) {
	config := &Config{}
	return config, yaml.Unmarshal(data, config)
}

// Load configuration files from a directory
func Load(dir string) *Config {
	out := &Config{}
	_ = filepath.Walk(dir, func(path string, info os.FileInfo, _ error) error {
		if info != nil && info.IsDir() {
			return nil
		}
		ext := filepath.Ext(path)
		if ext != ".json" && ext != ".yaml" && ext != ".yml" {
			return nil
		}
		data, err := ioutil.ReadFile(path)
		if err != nil {
			log.Printf("error loading file %q: %v\n", path, err)
			return nil
		}
		config, err := Read(data)
		if err != nil {
			log.Printf("error parsing file %q: %v\n", path, err)
			return nil
		}
		out.merge(config)
		return nil
	})
	return out
}

// Watch configuration files for changes (blocking call)
func Watch(dir string, apply func(*Config)) error {
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		return err
	}
	err = watcher.Add(dir)
	if err != nil {
		return err
	}
	defer watcher.Close()
	current := Load(dir)
	apply(current)
	for {
		select {
		case _, ok := <-watcher.Events:
			if !ok {
				return nil
			}
			next := Load(dir)
			if reflect.DeepEqual(next, current) {
				continue
			}
			current = next
			apply(current)
		case err, ok := <-watcher.Errors:
			if !ok {
				return nil
			}
			log.Printf("watch error: %v\n", err)
		}
	}
}
