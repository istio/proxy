/* Copyright 2018 The Bazel Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package proto

import "path/filepath"

// Package contains metadata for a set of .proto files that have the
// same package name. This translates to a proto_library rule.
type Package struct {
	Name        string
	RuleName    string // if not set, defaults to Name
	Files       map[string]FileInfo
	Imports     map[string]bool
	Options     map[string]string
	HasServices bool
}

func newPackage(name string) *Package {
	return &Package{
		Name:    name,
		Files:   map[string]FileInfo{},
		Imports: map[string]bool{},
		Options: map[string]string{},
	}
}

func (p *Package) addFile(info FileInfo) {
	p.Files[info.Name] = info
	for _, imp := range info.Imports {
		p.Imports[imp] = true
	}
	for _, opt := range info.Options {
		p.Options[opt.Key] = opt.Value
	}
	p.HasServices = p.HasServices || info.HasServices
}

func (p *Package) addGenFile(dir, name string) {
	p.Files[name] = FileInfo{
		Name: name,
		Path: filepath.Join(dir, filepath.FromSlash(name)),
	}
}
