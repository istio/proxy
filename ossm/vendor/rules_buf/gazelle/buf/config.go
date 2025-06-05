// Copyright 2021-2023 Buf Technologies, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package buf

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"os"
	"path"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/bazelbuild/bazel-gazelle/config"
	"github.com/bazelbuild/bazel-gazelle/label"
	"github.com/bazelbuild/bazel-gazelle/language/proto"
	"github.com/bazelbuild/bazel-gazelle/rule"
	"gopkg.in/yaml.v3"
)

var defaultBufConfigFiles = []string{
	"buf.yaml",
	"buf.mod",
}

func (*bufLang) RegisterFlags(flagSet *flag.FlagSet, cmd string, gazelleConfig *config.Config) {
	SetConfigForGazelleConfig(
		gazelleConfig,
		&Config{
			BreakingExcludeImports: true,
			BreakingMode:           BreakingModeModule,
		},
	)
}

func (*bufLang) CheckFlags(flagSet *flag.FlagSet, gazelleConfig *config.Config) error { return nil }

func (*bufLang) KnownDirectives() []string {
	return []string{
		"buf_breaking_mode",
		"buf_breaking_against",
		"buf_breaking_exclude_imports",
	}
}

func (*bufLang) Configure(gazelleConfig *config.Config, relativePath string, file *rule.File) {
	SetConfigForGazelleConfig(
		gazelleConfig,
		loadConfig(
			gazelleConfig,
			relativePath,
			file,
		),
	)
}

func loadConfig(gazelleConfig *config.Config, packageRelativePath string, file *rule.File) *Config {
	// Config is inherited from parent directory if we modify without making a copy
	// it will be polluted when traversing sibling directories.
	//
	// https://github.com/bazelbuild/bazel-gazelle/blob/master/Design.rst#configuration
	config := GetConfigForGazelleConfig(gazelleConfig).clone()
	config.ModuleRoot = false
	bufModule, bufConfigFile, err := loadDefaultBufModule(
		filepath.Join(
			gazelleConfig.RepoRoot,
			packageRelativePath,
		),
	)
	if err != nil {
		log.Print("error trying to load default config", err)
	}
	// log.Println("loadConfig", packageRelativePath)
	// Two cases where this could be the module root:
	// 1. If there is a buf.yaml file and no modules are defined.
	if bufModule != nil {
		config.Module = bufModule
		config.BufConfigFile = label.New("", packageRelativePath, bufConfigFile)
		config.ModuleRoot = len(bufModule.Modules) == 0
	}
	// 2. If the ancestor has a v2 buf.yaml file with matching path.
	if config.Module != nil && config.Module.Version == "v2" {
		for _, module := range config.Module.Modules {
			if module.Path == "." {
				module.Path = ""
			}
			if path.Join(config.BufConfigFile.Pkg, module.Path) == packageRelativePath {
				config.ModuleRoot = true
				config.ModuleConfig = &module
				break
			}
		}
	}
	// When using workspaces, for gazelle to generate accurate proto_library rules
	// we need add `# gazelle:proto_strip_import_prefix /path` to BUILD file at each module root
	//
	// Here we set the config if the directive is not present
	if config.ModuleRoot && packageRelativePath != "" {
		protoConfig := proto.GetProtoConfig(gazelleConfig)
		stripImportPrefix := "/" + packageRelativePath
		if protoConfig.StripImportPrefix == "" {
			protoConfig.StripImportPrefix = stripImportPrefix
		}
		if protoConfig.StripImportPrefix != stripImportPrefix {
			log.Printf(
				"strip_import_prefix at %s should be %s but is %s",
				packageRelativePath,
				stripImportPrefix,
				protoConfig.StripImportPrefix,
			)
		}
	}
	if file == nil {
		return config
	}
	for _, d := range file.Directives {
		switch d.Key {
		case "buf_breaking_against":
			config.BreakingImageTarget = d.Value
		case "buf_breaking_exclude_imports":
			value, err := strconv.ParseBool(strings.TrimSpace(d.Value))
			if err != nil {
				log.Printf("buf_breaking_exclude_imports directive should be a boolean got: %s", d.Value)
			}
			config.BreakingExcludeImports = value
		case "buf_breaking_mode":
			breakingMode, err := ParseBreakingMode(d.Value)
			if err != nil {
				log.Printf("error parsing buf_breaking_mode: %v", err)
			}
			config.BreakingMode = breakingMode
		}
	}
	return config
}

func readBufModuleConfig(file string) (*BufModule, error) {
	data, err := os.ReadFile(file)
	if err != nil {
		return nil, err
	}
	var bufModule BufModule
	return &bufModule, parseJsonOrYaml(data, &bufModule)
}

// parseJsonOrYaml follows buf's parsing of config:
// https://github.com/bufbuild/buf/blob/63520331f5c01b9f388d787c4e2959a9a299262c/private/pkg/encoding/encoding.go#L111
func parseJsonOrYaml(data []byte, v interface{}) error {
	if err := json.Unmarshal(data, v); err != nil {
		if err := yaml.Unmarshal(data, v); err != nil {
			return err
		}
	}
	return nil
}

func loadDefaultBufModule(workingDirectory string) (*BufModule, string, error) {
	for _, bufConfigFile := range defaultBufConfigFiles {
		bufModule, err := readBufModuleConfig(filepath.Join(workingDirectory, bufConfigFile))
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, "", fmt.Errorf("buf: unable to parse buf config file at %s, err: %w", bufConfigFile, err)
		}

		return bufModule, bufConfigFile, nil
	}
	return nil, "", nil
}

func isWithinExcludes(config *Config, path string) bool {
	if config.Module == nil {
		return false
	}
	// v1
	for _, exclude := range config.Module.Build.Excludes {
		if strings.Contains(path, exclude) {
			return true
		}
	}
	// v2, all paths are relative to the root to `buf.yaml`, so no need to filter out the module.
	for _, module := range config.Module.Modules {
		for _, exclude := range module.Excludes {
			if strings.HasPrefix(path, exclude) {
				return true
			}
		}
	}
	return false
}

func (c *Config) clone() *Config {
	configClone := *c
	if c.Module != nil {
		moduleClone := *c.Module
		configClone.Module = &moduleClone
	}
	return &configClone
}
