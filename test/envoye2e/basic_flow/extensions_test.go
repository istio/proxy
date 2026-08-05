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

package client_test

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"testing"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
	"istio.io/proxy/test/envoye2e/env"
)

// Extensions whose bzl dict key differs from their runtime-registered factory
// name. The test verifies these are present under their runtime name.
var runtimeNameOverrides = map[string]string{
	// Cluster factories register as "envoy.cluster.*" (singular), not "envoy.clusters.*"
	"envoy.clusters.dns":          "envoy.cluster.dns",
	"envoy.clusters.eds":          "envoy.cluster.eds",
	"envoy.clusters.logical_dns":  "envoy.cluster.logical_dns",
	"envoy.clusters.original_dst": "envoy.cluster.original_dst",
	"envoy.clusters.static":       "envoy.cluster.static",
	"envoy.clusters.strict_dns":   "envoy.cluster.strict_dns",
	// Upstream connection pool factories use a different naming scheme
	"envoy.upstreams.http.http": "envoy.filters.connection_pools.http.http",
	"envoy.upstreams.http.tcp":  "envoy.filters.connection_pools.http.tcp",
	// Istio ALPN filter registers as "istio.alpn"
	"envoy.filters.http.alpn": "istio.alpn",
	// Dynamic modules HTTP filter uses fully-qualified name
	"envoy.filters.http.dynamic_modules": "envoy.extensions.filters.http.dynamic_modules",
}

// Extensions present in the bzl dict but not verifiable via /server_info.
var excludedExtensions = map[string]string{
	// Bzl alias: same target registers as delta_grpc_collection, aggregated_grpc_collection,
	// and ads_collection — all verified via their own dict entries.
	"envoy.config_subscription.aggregated_delta_grpc_collection": "bzl alias; target's factories register under other dict keys",
	// Placeholder with no REGISTER_FACTORY (upstream TODO in config.h)
	"envoy.io_socket.user_space": "no factory registration (upstream TODO)",
	// Conditionally compiled for macOS only (select on //bazel:apple)
	"envoy.network.dns_resolver.apple": "macOS only",
	// WASM runtimes gated behind envoy_select_wasm_wamr / envoy_select_wasm_wasmtime
	"envoy.wasm.runtime.wamr":     "requires wasm_wamr build flag",
	"envoy.wasm.runtime.wasmtime": "requires wasm_wasmtime build flag",
	// Extra extension requiring ENVOY_HAS_EXTRA_EXTENSIONS=true at build time
	"envoy.filters.http.kill_request": "extra extension, not built by default",
	// Registers as ProactiveResourceMonitorFactory, not included in /server_info extension listing
	"envoy.resource_monitors.downstream_connections": "proactive resource monitor, not in /server_info",
}

// TestExtensionsBuildConfig verifies that the built Envoy binary contains all
// extensions defined in bazel/extension_config/extensions_build_config.bzl.
func TestExtensionsBuildConfig(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)

	bzlPath := filepath.Join(driver.BazelWorkspace(), "bazel/extension_config/extensions_build_config.bzl")
	configured, err := parseExpectedExtensions(bzlPath)
	if err != nil {
		t.Fatalf("failed to parse build config: %v", err)
	}
	if len(configured) == 0 {
		t.Fatal("parsed zero configured extensions; likely a parsing error")
	}
	t.Logf("parsed %d configured extensions from extensions_build_config.bzl", len(configured))

	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.Envoy{Bootstrap: `
node:
  id: extensions-test
admin:
  access_log_path: /dev/null
  address:
    socket_address:
      address: 127.0.0.1
      port_value: {{ .Ports.ServerAdmin }}
`},
			driver.StepFunction(func(p *driver.Params) error {
				return verifyExtensions(t, p.Ports.ServerAdmin, configured)
			}),
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}

func verifyExtensions(t *testing.T, adminPort uint16, configured []string) error {
	_, body, err := env.HTTPGet(fmt.Sprintf("http://127.0.0.1:%d/server_info", adminPort))
	if err != nil {
		return fmt.Errorf("failed to query /server_info: %w", err)
	}

	var info struct {
		Node struct {
			Extensions []struct {
				Name string `json:"name"`
			} `json:"extensions"`
		} `json:"node"`
	}
	if err := json.Unmarshal([]byte(body), &info); err != nil {
		return fmt.Errorf("failed to parse /server_info response: %w", err)
	}

	registered := make(map[string]bool, len(info.Node.Extensions))
	for _, ext := range info.Node.Extensions {
		registered[ext.Name] = true
	}
	log.Printf("binary reports %d registered extensions", len(registered))

	var missing []string
	verified := 0
	for _, bzlKey := range configured {
		if reason, ok := excludedExtensions[bzlKey]; ok {
			t.Logf("skipping %s: %s", bzlKey, reason)
			continue
		}

		runtimeName := bzlKey
		if override, ok := runtimeNameOverrides[bzlKey]; ok {
			runtimeName = override
		}

		if !registered[runtimeName] {
			if runtimeName != bzlKey {
				missing = append(missing, fmt.Sprintf("%s (looked up as %s)", bzlKey, runtimeName))
			} else {
				missing = append(missing, bzlKey)
			}
		} else {
			verified++
		}
	}

	if len(missing) > 0 {
		sort.Strings(missing)
		return fmt.Errorf("binary is missing %d configured extensions:\n  %s",
			len(missing), strings.Join(missing, "\n  "))
	}

	log.Printf("verified %d extensions present in binary", verified)
	return nil
}

// parseExpectedExtensions reads extensions_build_config.bzl and returns the
// EXTENSIONS dict keys: ENVOY_EXTENSIONS - ISTIO_DISABLED_EXTENSIONS + enabled contrib.
func parseExpectedExtensions(bzlPath string) ([]string, error) {
	data, err := os.ReadFile(bzlPath)
	if err != nil {
		return nil, err
	}
	content := string(data)

	envoyExts := parseBzlDictKeys(content, "ENVOY_EXTENSIONS")
	contribExts := parseBzlDictKeys(content, "ENVOY_CONTRIB_EXTENSIONS")
	disabledExts := parseBzlListItems(content, "ISTIO_DISABLED_EXTENSIONS")
	enabledContribExts := parseBzlListItems(content, "ISTIO_ENABLED_CONTRIB_EXTENSIONS")

	disabled := toSet(disabledExts)
	enabledContrib := toSet(enabledContribExts)

	var configured []string
	for _, e := range envoyExts {
		if !disabled[e] {
			configured = append(configured, e)
		}
	}
	for _, e := range contribExts {
		if enabledContrib[e] {
			configured = append(configured, e)
		}
	}

	sort.Strings(configured)
	return configured, nil
}

func toSet(items []string) map[string]bool {
	s := make(map[string]bool, len(items))
	for _, item := range items {
		s[item] = true
	}
	return s
}

// parseBzlDictKeys extracts keys from a Starlark dict: VAR = { "key": "value", ... }
func parseBzlDictKeys(content, varName string) []string {
	startRe := regexp.MustCompile(`\b` + regexp.QuoteMeta(varName) + `\s*=\s*\{`)
	keyRe := regexp.MustCompile(`^\s*"([^"]+)"\s*:`)

	var keys []string
	inDict := false
	for _, line := range strings.Split(content, "\n") {
		if !inDict {
			if startRe.MatchString(line) {
				inDict = true
			}
			continue
		}
		if strings.TrimSpace(line) == "}" {
			break
		}
		if m := keyRe.FindStringSubmatch(line); m != nil {
			keys = append(keys, m[1])
		}
	}
	return keys
}

// parseBzlListItems extracts items from a Starlark list: VAR = [ "item", ... ]
func parseBzlListItems(content, varName string) []string {
	startRe := regexp.MustCompile(`\b` + regexp.QuoteMeta(varName) + `\s*=\s*\[`)
	itemRe := regexp.MustCompile(`^\s*"([^"]+)"`)

	var items []string
	inList := false
	for _, line := range strings.Split(content, "\n") {
		if !inList {
			if startRe.MatchString(line) {
				inList = true
			}
			continue
		}
		if strings.TrimSpace(line) == "]" {
			break
		}
		if m := itemRe.FindStringSubmatch(line); m != nil {
			items = append(items, m[1])
		}
	}
	return items
}
