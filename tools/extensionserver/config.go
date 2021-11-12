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
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"reflect"

	"github.com/fsnotify/fsnotify"
	"sigs.k8s.io/yaml"
)

// Config for the extension server
type Config struct {
	// Extensions list
	Extensions []*Extension `json:"extensions"`
}

// Extension configuration
type Extension struct {
	// Name of the extension. Must match the resource name in ECDS
	Name string `json:"name"`

	// Configuration passed as JSON string.
	Configuration json.RawMessage `json:"configuration"`

	// Path to the extension code (one of path or URL must be specified).
	Path string `json:"path,omitempty"`

	// URL to the extension code (one of path or URL must be specified).
	URL string `json:"url,omitempty"`

	// SHA256 of the content (optional)
	SHA256 string `json:"sha256,omitempty"`

	// VMID (optional).
	VMID string `json:"vm_id,omitempty"`

	// RootID (optional).
	RootID string `json:"root_id,omitempty"`

	// Runtime (optional, defaults to v8).
	Runtime string `json:"runtime,omitempty"`

	// Optional comment
	Comment string `json:"comment,omitempty"`
}

func (config *Config) merge(that *Config) {
	config.Extensions = append(config.Extensions, that.Extensions...)
}

func (config *Config) validate() []error {
	var out []error
	for _, extension := range config.Extensions {
		if errors := extension.validate(); len(errors) > 0 {
			out = append(out, errors...)
		}
	}
	return out
}

func (ext *Extension) validate() []error {
	var out []error
	if ext.Path != "" && ext.URL != "" || ext.Path == "" && ext.URL == "" {
		out = append(out, fmt.Errorf("exactly one of 'path' and 'url' must be set"))
	}
	if ext.Name == "" {
		out = append(out, fmt.Errorf("'name' is required"))
	}
	return out
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
		if errs := config.validate(); len(errs) > 0 {
			log.Printf("validation error in file %q: %v\n", path, errs)
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
