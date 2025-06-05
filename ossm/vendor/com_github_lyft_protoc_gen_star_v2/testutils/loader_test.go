package testutils

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
	"testing"

	"github.com/spf13/afero"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

func TestResolveProtoc(t *testing.T) {
	t.Parallel()

	lookupPath, lookupErr := exec.LookPath("protoc")

	t.Run("from PATH", func(t *testing.T) {
		t.Parallel()

		l := Loader{}
		mt := &mockT{}

		if lookupErr != nil {
			t.Skip("no protoc in PATH: ", lookupErr)
			return
		}

		assert.Equal(t, lookupPath, l.resolveProtoc(mt))
		assert.False(t, mt.failed)
	})

	t.Run("explicit", func(t *testing.T) {
		t.Parallel()

		if lookupErr != nil {
			t.Skip("no protoc in PATH: ", lookupErr)
			return
		}

		l := Loader{Protoc: lookupPath}
		mt := &mockT{}

		assert.Equal(t, lookupPath, l.resolveProtoc(mt))
		assert.False(t, mt.failed)
	})

	t.Run("invalid", func(t *testing.T) {
		t.Parallel()

		l := Loader{Protoc: "/this/is/not/a/real/protoc"}
		mt := &mockT{}

		l.resolveProtoc(mt)
		assert.True(t, mt.failed)
	})
}

func TestResolveFS(t *testing.T) {
	t.Parallel()

	l := Loader{}
	assert.IsType(t, afero.NewOsFs(), l.resolveFS())

	fs := afero.NewMemMapFs()
	l = Loader{FS: fs}
	assert.Equal(t, fs, l.resolveFS())
}

func TestResolveTargets(t *testing.T) {
	t.Parallel()

	fs := afero.NewMemMapFs()

	require.NoError(t, fs.Mkdir("foo", 0755))

	files := []string{
		"foo/bar.proto",
		"foo/baz.proto",
		"fizz.proto",
		"buzz.proto",
	}

	for _, path := range files {
		f, err := fs.Create(path)
		require.NoError(t, err)
		require.NoError(t, f.Close())
	}

	l := Loader{FS: fs}

	t.Run("empty", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		targets := l.resolveTargets(mt)
		assert.Empty(t, targets)
		assert.True(t, mt.failed)
	})

	t.Run("empty glob", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		targets := l.resolveTargets(mt, "/not/a/real/*.proto")
		assert.Empty(t, targets)
		assert.True(t, mt.failed)
	})

	t.Run("bad glob", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		targets := l.resolveTargets(mt, "[-]")
		assert.Empty(t, targets)
		assert.True(t, mt.failed)
	})

	t.Run("success", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		targets := l.resolveTargets(mt, "*/*.proto", "fizz.proto")
		assert.Len(t, targets, 3)
		assert.Contains(t, targets, "foo/bar.proto")
		assert.Contains(t, targets, "foo/baz.proto")
		assert.Contains(t, targets, "fizz.proto")
		assert.False(t, mt.failed)
	})
}

func TestResolveArgs(t *testing.T) {
	t.Parallel()

	l := Loader{
		ImportPaths: []string{"/foo", "/bar"},
	}

	args := l.resolveArgs("fdset.bin", []string{"fizz.proto", "buzz.proto"})
	expected := []string{
		"-o", "fdset.bin",
		"--include_imports",
		"--include_source_info",
		"-I", "/foo",
		"-I", "/bar",
		"fizz.proto",
		"buzz.proto",
	}
	assert.Equal(t, expected, args)
}

func TestWithTempDir(t *testing.T) {
	t.Parallel()

	t.Run("success", func(t *testing.T) {
		t.Parallel()

		fs := afero.NewMemMapFs()
		l := Loader{FS: fs}
		mt := &mockT{}
		var dir string

		l.withTempDir(mt, func(tempDir string) {
			info, err := fs.Stat(tempDir)
			require.NoError(t, err)
			assert.True(t, info.IsDir())
			dir = tempDir
		})

		assert.False(t, mt.failed)

		_, err := fs.Stat(dir)
		assert.Error(t, err)

	})

	t.Run("failure", func(t *testing.T) {
		t.Parallel()

		fs := afero.NewMemMapFs()
		fs = afero.NewReadOnlyFs(fs)
		l := Loader{FS: fs}
		mt := &mockT{}

		l.withTempDir(mt, func(tempDir string) {
			assert.Fail(t, "should not have reached here")
		})

		assert.True(t, mt.failed)
	})

	t.Run("fail cleanup", func(t *testing.T) {
		t.Parallel()

		fs := afero.NewMemMapFs()
		fs = disallowRemoveAllFS{Fs: fs}
		l := Loader{FS: fs}
		mt := &mockT{}

		var executed bool
		l.withTempDir(mt, func(tempDir string) {
			executed = true
		})

		assert.True(t, executed)
		assert.False(t, mt.failed)
		assert.NotEmpty(t, mt.log)
	})
}

func TestLoader_LoadFDSetReader(t *testing.T) {
	t.Parallel()

	raw, err := proto.Marshal(dummyFDSet())
	require.NoError(t, err)

	t.Run("success - no bidi", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		l := Loader{}
		r := bytes.NewReader(raw)

		ast := l.LoadFDSetReader(mt, r)
		assert.False(t, mt.failed)
		assert.NotNil(t, ast)
		assert.NotEmpty(t, ast.Packages())
	})

	t.Run("success - bidi", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		l := Loader{BiDirectional: true}
		r := bytes.NewReader(raw)

		ast := l.LoadFDSetReader(mt, r)
		assert.False(t, mt.failed)
		assert.NotNil(t, ast)
		assert.NotEmpty(t, ast.Packages())
	})

	t.Run("broken reader", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		r := brokenReader{}
		l := Loader{}

		ast := l.LoadFDSetReader(mt, r)
		assert.Nil(t, ast)
		assert.True(t, mt.failed)
	})

	t.Run("unmarshal error", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		r := strings.NewReader("this is not a fdset")
		l := Loader{}

		ast := l.LoadFDSetReader(mt, r)
		assert.Nil(t, ast)
		assert.True(t, mt.failed)
	})

	t.Run("process error", func(t *testing.T) {
		typ := descriptor.FieldDescriptorProto_TYPE_MESSAGE
		msg := &descriptor.DescriptorProto{
			Name: proto.String("SomeMsg"),
			Field: []*descriptor.FieldDescriptorProto{{
				Name:     proto.String("SomeName"),
				Type:     &typ,
				TypeName: proto.String(".some.unknown.Message"),
			}},
		}
		fdset := dummyFDSet()
		fdset.File[0].MessageType = []*descriptor.DescriptorProto{msg}

		b, pberr := proto.Marshal(fdset)
		require.NoError(t, pberr)

		mt := &mockT{}
		r := bytes.NewReader(b)
		l := Loader{}

		ast := l.LoadFDSetReader(mt, r)
		assert.Nil(t, ast)
		assert.True(t, mt.failed)
	})
}

func TestLoader_LoadFDSet(t *testing.T) {
	t.Parallel()

	raw, err := proto.Marshal(dummyFDSet())
	require.NoError(t, err)

	fs := afero.NewMemMapFs()
	path := "/fdset.bin"
	err = afero.WriteFile(fs, path, raw, 0644)
	require.NoError(t, err)

	l := Loader{FS: fs}

	t.Run("success", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}

		ast := l.LoadFDSet(mt, path)
		assert.NotNil(t, ast)
		assert.False(t, mt.failed)
	})

	t.Run("cannot open", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}

		ast := l.LoadFDSet(mt, "/not-a-real.proto")
		assert.Nil(t, ast)
		assert.True(t, mt.failed)
	})

	t.Run("cannot close file", func(t *testing.T) {
		t.Parallel()

		mt := &mockT{}
		ldr := Loader{FS: disallowCloseFileFS{fs}}

		ast := ldr.LoadFDSet(mt, path)
		assert.NotNil(t, ast)
		assert.False(t, mt.failed)
		assert.NotEmpty(t, mt.log)
	})
}

func TestLoader_LoadProtos(t *testing.T) {
	t.Parallel()

	if _, err := exec.LookPath("protoc"); err != nil {
		t.Skip("protoc not found in PATH")
		return
	}

	t.Run("non OS FS", func(t *testing.T) {
		t.Parallel()

		l := Loader{FS: afero.NewMemMapFs()}
		mt := &mockT{}

		ast := l.LoadProtos(mt, "*.proto")
		assert.Nil(t, ast)
		assert.True(t, mt.failed)
	})

	t.Run("success", func(t *testing.T) {
		t.Parallel()

		l := Loader{ImportPaths: []string{"../testdata/protos"}}
		mt := &mockT{}

		ast := l.LoadProtos(mt, "../testdata/protos/kitchen/*.proto")
		assert.NotNil(t, ast)
		assert.False(t, mt.failed)
	})

	t.Run("protoc error", func(t *testing.T) {
		t.Parallel()

		l := Loader{}
		mt := &mockT{}

		ast := l.LoadProtos(mt, "../testdata/protos/kitchen/kitchen.proto")
		assert.Nil(t, ast)
		assert.True(t, mt.failed)
	})
}

func dummyFDSet() *descriptor.FileDescriptorSet {
	f := &descriptor.FileDescriptorProto{
		Name:    proto.String("foo.proto"),
		Package: proto.String("testutil"),
		Syntax:  proto.String("proto3"),
	}

	return &descriptor.FileDescriptorSet{
		File: []*descriptor.FileDescriptorProto{f},
	}
}

type mockT struct {
	log    string
	failed bool
}

func (m *mockT) Logf(format string, args ...interface{}) {
	m.log = fmt.Sprintf(format, args...)
}

func (m *mockT) Fatal(args ...interface{}) {
	m.failed = true
	m.log = fmt.Sprint(args...)
}

func (m *mockT) Fatalf(format string, args ...interface{}) {
	m.failed = true
	m.log = fmt.Sprintf(format, args...)
}

type disallowRemoveAllFS struct {
	afero.Fs
}

func (disallowRemoveAllFS) RemoveAll(_ string) error {
	return os.ErrPermission
}

type brokenReader struct{}

func (b brokenReader) Read(_ []byte) (int, error) {
	return 0, io.ErrUnexpectedEOF
}

type disallowCloseFile struct {
	afero.File
}

func (disallowCloseFile) Close() error {
	return errors.New("cannot close file")
}

type disallowCloseFileFS struct {
	afero.Fs
}

func (fs disallowCloseFileFS) Open(path string) (afero.File, error) {
	file, err := fs.Fs.Open(path)
	if err != nil {
		return file, err
	}

	return disallowCloseFile{file}, nil
}

var (
	_ T = (*testing.T)(nil)
	_ T = (*testing.B)(nil)
)
