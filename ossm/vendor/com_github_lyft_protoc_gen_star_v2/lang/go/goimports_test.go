package pgsgo

import (
	"testing"

	"github.com/stretchr/testify/assert"

	pgs "github.com/lyft/protoc-gen-star/v2"
)

func TestGoImports_Match(t *testing.T) {
	t.Parallel()

	pp := GoImports()

	tests := []struct {
		n string
		a pgs.Artifact
		m bool
	}{
		{"GenFile", pgs.GeneratorFile{Name: "foo.go"}, true},
		{"GenFileNonGo", pgs.GeneratorFile{Name: "bar.txt"}, false},

		{"GenTplFile", pgs.GeneratorTemplateFile{Name: "foo.go"}, true},
		{"GenTplFileNonGo", pgs.GeneratorTemplateFile{Name: "bar.txt"}, false},

		{"CustomFile", pgs.CustomFile{Name: "foo.go"}, true},
		{"CustomFileNonGo", pgs.CustomFile{Name: "bar.txt"}, false},

		{"CustomTplFile", pgs.CustomTemplateFile{Name: "foo.go"}, true},
		{"CustomTplFileNonGo", pgs.CustomTemplateFile{Name: "bar.txt"}, false},

		{"NonMatch", pgs.GeneratorAppend{FileName: "foo.go"}, false},
	}

	for _, test := range tests {
		tc := test
		t.Run(tc.n, func(t *testing.T) {
			t.Parallel()
			assert.Equal(t, tc.m, pp.Match(tc.a))
		})
	}
}

func TestGoImports_Process(t *testing.T) {
	t.Parallel()

	src := []byte("package foo\n\nimport (\n\t\"fmt\"\n\t\"strings\"\n)\n\nfunc Hello() {\n\tfmt.Println(\"Hello\")\n}\n")
	exp := []byte("package foo\n\nimport (\n\t\"fmt\"\n)\n\nfunc Hello() {\n\tfmt.Println(\"Hello\")\n}\n")

	out, err := GoImports().Process(src)
	assert.NoError(t, err)
	assert.Equal(t, string(exp), string(out))
}
