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

/*
generate.go is a program that generates the Gazelle YAML manifest.

The Gazelle manifest is a file that contains extra information required when
generating the Bazel BUILD files.
*/
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/bazel-contrib/rules_python/gazelle/manifest"
)

func main() {
	var (
		manifestGeneratorHashPath string
		requirementsPath          string
		pipRepositoryName         string
		modulesMappingPath        string
		outputPath                string
		updateTarget              string
	)
	flag.StringVar(
		&manifestGeneratorHashPath,
		"manifest-generator-hash",
		"",
		"The file containing the hash for the source code of the manifest generator."+
			"This is important to force manifest updates when the generator logic changes.")
	flag.StringVar(
		&requirementsPath,
		"requirements",
		"",
		"The requirements.txt file.")
	flag.StringVar(
		&pipRepositoryName,
		"pip-repository-name",
		"",
		"The name of the pip_parse or pip.parse target.")
	flag.StringVar(
		&modulesMappingPath,
		"modules-mapping",
		"",
		"The modules_mapping.json file.")
	flag.StringVar(
		&outputPath,
		"output",
		"",
		"The output YAML manifest file.")
	flag.StringVar(
		&updateTarget,
		"update-target",
		"",
		"The Bazel target to update the YAML manifest file.")
	flag.Parse()

	if modulesMappingPath == "" {
		log.Fatalln("ERROR: --modules-mapping must be set")
	}

	if outputPath == "" {
		log.Fatalln("ERROR: --output must be set")
	}

	if updateTarget == "" {
		log.Fatalln("ERROR: --update-target must be set")
	}

	modulesMapping, err := unmarshalJSON(modulesMappingPath)
	if err != nil {
		log.Fatalf("ERROR: %v\n", err)
	}

	header := generateHeader(updateTarget)
	repository := manifest.PipRepository{
		Name: pipRepositoryName,
	}

	manifestFile := manifest.NewFile(&manifest.Manifest{
		ModulesMapping: modulesMapping,
		PipRepository:  &repository,
	})
	if err := writeOutput(
		outputPath,
		header,
		manifestFile,
		manifestGeneratorHashPath,
		requirementsPath,
	); err != nil {
		log.Fatalf("ERROR: %v\n", err)
	}
}

// unmarshalJSON returns the parsed mapping from the given JSON file path.
func unmarshalJSON(jsonPath string) (map[string]string, error) {
	file, err := os.Open(jsonPath)
	if err != nil {
		return nil, fmt.Errorf("failed to unmarshal JSON file: %w", err)
	}
	defer file.Close()

	decoder := json.NewDecoder(file)
	output := make(map[string]string)
	if err := decoder.Decode(&output); err != nil {
		return nil, fmt.Errorf("failed to unmarshal JSON file: %w", err)
	}

	return output, nil
}

// generateHeader generates the YAML header human-readable comment.
func generateHeader(updateTarget string) string {
	var header strings.Builder
	header.WriteString("# GENERATED FILE - DO NOT EDIT!\n")
	header.WriteString("#\n")
	header.WriteString("# To update this file, run:\n")
	header.WriteString(fmt.Sprintf("#   bazel run %s\n", updateTarget))
	return header.String()
}

// writeOutput writes to the final file the header and manifest structure.
func writeOutput(
	outputPath string,
	header string,
	manifestFile *manifest.File,
	manifestGeneratorHashPath string,
	requirementsPath string,
) error {
	outputFile, err := os.OpenFile(outputPath, os.O_WRONLY|os.O_TRUNC|os.O_CREATE, 0644)
	if err != nil {
		return fmt.Errorf("failed to write output: %w", err)
	}
	defer outputFile.Close()

	if _, err := fmt.Fprintf(outputFile, "%s\n---\n", header); err != nil {
		return fmt.Errorf("failed to write output: %w", err)
	}

	if requirementsPath != "" {
		manifestGeneratorHash, err := os.Open(manifestGeneratorHashPath)
		if err != nil {
			return fmt.Errorf("failed to write output: %w", err)
		}
		defer manifestGeneratorHash.Close()

		requirements, err := os.Open(requirementsPath)
		if err != nil {
			return fmt.Errorf("failed to write output: %w", err)
		}
		defer requirements.Close()

		if err := manifestFile.EncodeWithIntegrity(outputFile, manifestGeneratorHash, requirements); err != nil {
			return fmt.Errorf("failed to write output: %w", err)
		}
	} else {
		if err := manifestFile.EncodeWithoutIntegrity(outputFile); err != nil {
			return fmt.Errorf("failed to write output: %w", err)
		}
	}

	return nil
}
