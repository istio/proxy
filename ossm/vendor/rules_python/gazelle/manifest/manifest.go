// Copyright 2023 The Bazel Authors. All rights reserved.
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

package manifest

import (
	"crypto/sha256"
	"fmt"
	"io"
	"os"

	"github.com/emirpasic/gods/sets/treeset"

	yaml "gopkg.in/yaml.v2"
)

// File represents the gazelle_python.yaml file.
type File struct {
	Manifest *Manifest `yaml:"manifest,omitempty"`
	// Integrity is the hash of the requirements.txt file and the Manifest for
	// ensuring the integrity of the entire gazelle_python.yaml file. This
	// controls the testing to keep the gazelle_python.yaml file up-to-date.
	Integrity string `yaml:"integrity,omitempty"`
}

// NewFile creates a new File with a given Manifest.
func NewFile(manifest *Manifest) *File {
	return &File{Manifest: manifest}
}

// Encode encodes the manifest file to the given writer.
func (f *File) EncodeWithIntegrity(w io.Writer, manifestGeneratorHashFile, requirements io.Reader) error {
	integrityBytes, err := f.calculateIntegrity(manifestGeneratorHashFile, requirements)
	if err != nil {
		return fmt.Errorf("failed to encode manifest file: %w", err)
	}
	f.Integrity = fmt.Sprintf("%x", integrityBytes)

	return f.encode(w)
}

func (f *File) EncodeWithoutIntegrity(w io.Writer) error {
	return f.encode(w)
}

func (f *File) encode(w io.Writer) error {
	encoder := yaml.NewEncoder(w)
	defer encoder.Close()
	if err := encoder.Encode(f); err != nil {
		return fmt.Errorf("failed to encode manifest file: %w", err)
	}
	return nil
}

// VerifyIntegrity verifies if the integrity set in the File is valid.
func (f *File) VerifyIntegrity(manifestGeneratorHashFile, requirements io.Reader) (bool, error) {
	integrityBytes, err := f.calculateIntegrity(manifestGeneratorHashFile, requirements)
	if err != nil {
		return false, fmt.Errorf("failed to verify integrity: %w", err)
	}
	valid := (f.Integrity == fmt.Sprintf("%x", integrityBytes))
	return valid, nil
}

// calculateIntegrity calculates the integrity of the manifest file based on the
// provided checksum for the requirements.txt file used as input to the modules
// mapping, plus the manifest structure in the manifest file. This integrity
// calculation ensures the manifest files are kept up-to-date.
func (f *File) calculateIntegrity(
	manifestGeneratorHash, requirements io.Reader,
) ([]byte, error) {
	hash := sha256.New()

	// Sum the manifest part of the file.
	encoder := yaml.NewEncoder(hash)
	defer encoder.Close()
	if err := encoder.Encode(f.Manifest); err != nil {
		return nil, fmt.Errorf("failed to calculate integrity: %w", err)
	}

	// Sum the manifest generator checksum bytes.
	if _, err := io.Copy(hash, manifestGeneratorHash); err != nil {
		return nil, fmt.Errorf("failed to calculate integrity: %w", err)
	}

	// Sum the requirements.txt checksum bytes.
	if _, err := io.Copy(hash, requirements); err != nil {
		return nil, fmt.Errorf("failed to calculate integrity: %w", err)
	}

	return hash.Sum(nil), nil
}

// Decode decodes the manifest file from the given path.
func (f *File) Decode(manifestPath string) error {
	file, err := os.Open(manifestPath)
	if err != nil {
		return fmt.Errorf("failed to decode manifest file: %w", err)
	}
	defer file.Close()

	decoder := yaml.NewDecoder(file)
	if err := decoder.Decode(f); err != nil {
		return fmt.Errorf("failed to decode manifest file: %w", err)
	}

	return nil
}

// ModulesMapping is the type used to map from importable Python modules to
// the wheel names that provide these modules.
type ModulesMapping map[string]string

// MarshalYAML makes sure that we sort the module names before marshaling
// the contents of `ModulesMapping` to a YAML file. This ensures that the
// file is deterministically generated from the map.
func (m ModulesMapping) MarshalYAML() (interface{}, error) {
	var mapslice yaml.MapSlice
	keySet := treeset.NewWithStringComparator()
	for key := range m {
		keySet.Add(key)
	}
	for _, key := range keySet.Values() {
		mapslice = append(mapslice, yaml.MapItem{Key: key, Value: m[key.(string)]})
	}
	return mapslice, nil
}

// Manifest represents the structure of the Gazelle manifest file.
type Manifest struct {
	// ModulesMapping is the mapping from importable modules to which Python
	// wheel name provides these modules.
	ModulesMapping ModulesMapping `yaml:"modules_mapping"`
	// PipDepsRepositoryName is the name of the pip_parse repository target.
	// DEPRECATED
	PipDepsRepositoryName string `yaml:"pip_deps_repository_name,omitempty"`
	// PipRepository contains the information for pip_parse or pip_repository
	// target.
	PipRepository *PipRepository `yaml:"pip_repository,omitempty"`
}

type PipRepository struct {
	// The name of the pip_parse or pip_repository target.
	Name string
}
