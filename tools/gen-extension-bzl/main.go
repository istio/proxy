/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"html/template"
	"io"
	"net/http"
	"os"
	"regexp"
)

//go:embed extensions_build_config.bzl.tpl
var extBuildTmp string

type ExtensionsOptions struct {
	EnvoyExtensions template.HTML
}

func main() {
	envoyOrg := env("ENVOY_ORG", "envoyproxy")
	envoyRepo := env("ENVOY_REPO", "envoy")
	envoyCommitID := env("ENVOY_LATEST_SHA", "main")
	extBuildConfigFile := env("EXTENSIONS_BUILD_CONFIG", "bazel/extension_config/extensions_build_config.bzl")
	f, err := readEnvoyExtensions(envoyOrg, envoyRepo, envoyCommitID)
	if err != nil {
		fmt.Println("read envoy fail, ", err)
		os.Exit(-1)
	}

	opts := ExtensionsOptions{
		EnvoyExtensions: template.HTML(f),
	}

	b, err := eval(opts)
	if err != nil {
		fmt.Println("eval template fail, ", err)
		os.Exit(-1)
	}

	if err := os.WriteFile(extBuildConfigFile, b, 0x755); err != nil {
		fmt.Println("wirte extensions file fail, ", err)
		os.Exit(-1)
	}
}

func eval(opts ExtensionsOptions) ([]byte, error) {
	t := template.New("ext_bzl")
	tpl, err := t.Parse(extBuildTmp)
	if err != nil {
		return nil, err
	}

	var b bytes.Buffer
	if err := tpl.Execute(&b, opts); err != nil {
		return nil, err
	}

	return b.Bytes(), nil
}

func env(key, defaultVal string) string {
	v, ok := os.LookupEnv(key)
	if !ok {
		return defaultVal
	}
	return v
}

func readEnvoyExtensions(envoyOrg, envoyRepo, commitid string) (string, error) {
	uri := fmt.Sprintf(`https://raw.githubusercontent.com/%s/%s/%s/source/extensions/extensions_build_config.bzl`,
		envoyOrg, envoyRepo, commitid)
	resp, err := http.Get(uri)
	if err != nil {
		return "", err
	}
	defer func() {
		resp.Body.Close()
	}()

	b, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	lines := string(b)
	r := regexp.MustCompile(`EXTENSIONS = {([\d\D]*)}`)
	out := r.FindString(lines)
	return out, nil
}
