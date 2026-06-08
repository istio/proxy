package pgsgo

import (
	"strings"

	"golang.org/x/tools/imports"

	pgs "github.com/lyft/protoc-gen-star/v2"
)

type goImports struct{}

// GoImports returns a PostProcessor that run goimports on any files ending . ".go"
func GoImports() pgs.PostProcessor { return goImports{} }

func (g goImports) Match(a pgs.Artifact) bool {
	var n string

	switch a := a.(type) {
	case pgs.GeneratorFile:
		n = a.Name
	case pgs.GeneratorTemplateFile:
		n = a.Name
	case pgs.CustomFile:
		n = a.Name
	case pgs.CustomTemplateFile:
		n = a.Name
	default:
		return false
	}

	return strings.HasSuffix(n, ".go")
}

func (g goImports) Process(in []byte) ([]byte, error) {
	// We do not want to give a filename here, ever.
	return imports.Process("", in, nil)
}

var _ pgs.PostProcessor = goImports{}
