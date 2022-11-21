package main

import (
	"fmt"
	"html/template"
	"os"
	"path"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestReadEnvoyExtensionsFromUpstream(t *testing.T) {
	commitid := "e368c3e5e6dcbcb893364dda99b71ff9d68f3158"
	got, err := readEnvoyExtensions("envoyproxy", "envoy", commitid)
	assert.NoError(t, err)

	expected := readOut(commitid)
	assert.Equal(t, expected, got)

	bzl, err := eval(ExtensionsOptions{
		EnvoyExtensions: template.HTML(got),
	})
	assert.NoError(t, err)
	expectedBzl := readBzl(commitid)
	assert.Equal(t, expectedBzl, string(bzl))
}

func readOut(commitid string) string {
	b, _ := os.ReadFile(path.Join("testdata", fmt.Sprintf("%s.out", commitid)))
	return string(b)
}

func readBzl(commitid string) string {
	b, _ := os.ReadFile(path.Join("testdata", fmt.Sprintf("%s.bzl", commitid)))
	return string(b)
}
