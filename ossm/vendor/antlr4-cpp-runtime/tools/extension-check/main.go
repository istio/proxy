// Copyright 2020 Istio Authors
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

package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"strings"

	"go.starlark.net/starlark"
)

var (
	wellknownIgnoreExtensions  = flag.String("ignore-extensions", "wellknown-extensions", "file contains list of extensions to ignore")
	envoyExtensionsBuildConfig = flag.String("envoy-extensions-build-config", "envoy_extensions_build_config.bzl", "envoy extensions build config file")
	proxyExtensionsBuildConfig = flag.String("proxy-extensions-build-config", "proxy_extensions_build_config.bzl", "proxy extensions build config file")
)

func main() {
	flag.Parse()
	envoyCoreExtensions, err := extensions(*envoyExtensionsBuildConfig, "EXTENSIONS")
	if err != nil {
		fmt.Printf("error: %v", err)
		os.Exit(1)
	}

	proxyExtensions, err := extensions(*proxyExtensionsBuildConfig, "ENVOY_EXTENSIONS")
	if err != nil {
		fmt.Printf("error: %v", err)
		os.Exit(1)
	}

	for k, expected := range envoyCoreExtensions {
		if actual, ok := proxyExtensions[k]; ok && expected == actual {
			delete(envoyCoreExtensions, k)
		}
	}

	ignoreExtensions := []string{}
	if _, err := os.Stat(*wellknownIgnoreExtensions); err == nil {
		if b, err := os.ReadFile(*wellknownIgnoreExtensions); err == nil {
			wellkonwns := strings.Split(string(b), "\n")
			ignoreExtensions = append(ignoreExtensions, wellkonwns...)
		}

		fmt.Println("ignore extensions: ", len(ignoreExtensions))

		for _, ext := range ignoreExtensions {
			delete(envoyCoreExtensions, ext)
		}
	} else {
		fmt.Println(err)
	}

	if len(envoyCoreExtensions) > 0 {
		for k, v := range envoyCoreExtensions {
			fmt.Printf("missing extension: %v: %v\n", k, v)
		}
		os.Exit(1)
	}
}

// extensions returns a map of extensions from the given file with giving key.
// The file is expected to be a starlark file that defines a global variable.
// Depends on go.starlark.net/starlark.
func extensions(filename, key string) (map[string]string, error) {
	thread := &starlark.Thread{
		Name:  "extensions",
		Print: func(_ *starlark.Thread, msg string) { fmt.Println(msg) },
	}
	globals, err := starlark.ExecFile(thread, filename, nil, nil)
	if err != nil {
		panic(err)
	}

	if v, ok := globals[key]; ok {
		extensions := map[string]string{}

		if err := json.Unmarshal([]byte(v.String()), &extensions); err != nil {
			return nil, err
		}

		return extensions, nil
	}

	return nil, errors.New("no extensions found")
}
