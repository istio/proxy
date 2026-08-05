package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestExtensionsIgnoresLoadStatements(t *testing.T) {
	t.Parallel()

	dir := t.TempDir()
	path := filepath.Join(dir, "extensions.bzl")
	content := `load(
    "@bazel_skylib//lib:dicts.bzl",
    "dicts",
)

EXTENSIONS = {"envoy.filters.http.router": "//source/extensions/filters/http/router:config"}
`
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}

	got, err := extensions(path, "EXTENSIONS")
	if err != nil {
		t.Fatalf("extensions() error = %v", err)
	}

	if got["envoy.filters.http.router"] != "//source/extensions/filters/http/router:config" {
		t.Fatalf("extensions() returned unexpected map: %#v", got)
	}
}

func TestExtensionsReturnsErrorForMissingFile(t *testing.T) {
	t.Parallel()

	if _, err := extensions(filepath.Join(t.TempDir(), "missing.bzl"), "EXTENSIONS"); err == nil {
		t.Fatal("extensions() error = nil, want missing file error")
	}
}
