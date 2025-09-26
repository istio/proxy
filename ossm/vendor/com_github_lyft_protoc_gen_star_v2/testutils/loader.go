package testutils

import (
	"io"
	"io/ioutil"
	"os/exec"
	"path/filepath"

	pgs "github.com/lyft/protoc-gen-star/v2"
	"github.com/spf13/afero"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

// The T interface represents a reduced API of the testing.T and testing.B
// standard library types used by the Loader.
type T interface {
	Logf(format string, args ...interface{})
	Fatal(args ...interface{})
	Fatalf(format string, args ...interface{})
}

// Loader is a testing utility that can resolve an AST in a variety of manners.
// The loader can be used to provide entities to test functions.
type Loader struct {
	// Protoc specifies the path to the `protoc` executable. If empty, the Loader
	// attempts to execute protoc via PATH.
	Protoc string

	// ImportPaths includes any extra -I (or --proto_path) flags to the protoc
	// execution required to resolve all proto dependencies.
	ImportPaths []string

	// BiDirectional specifies whether or not the AST should be resolved with
	// bidirectional AST resolution.
	BiDirectional bool

	// FS overrides the file system used by the Loader. FS must be nil or an
	// instance of *afero.OsFs if LoadProtos is called.
	FS afero.Fs
}

// LoadProtos executes protoc against the provided files (or globs, as defined
// by filepath.Glob), returning a resolved pgs.AST. The test/benchmark is
// fatally stopped if there is any error.
//
// This function requires the Loader's FS field to be nil or an instance of
// *afero.OsFs, otherwise, t will be immediately failed.
func (l Loader) LoadProtos(t T, files ...string) (ast pgs.AST) {
	switch l.FS.(type) {
	case nil, *afero.OsFs:
	// noop
	default:
		t.Fatal("cannot use LoadProtos with a non-OS file system")
		return nil
	}

	protoc := l.resolveProtoc(t)
	targets := l.resolveTargets(t, files...)

	l.withTempDir(t, func(tmpDir string) {
		tmpFile := filepath.Join(tmpDir, "fdset.bin")
		args := l.resolveArgs(tmpFile, targets)

		if out, err := exec.Command(protoc, args...).CombinedOutput(); err != nil {
			t.Fatalf("protoc execution failed with the following error: %v | Std Out/Err: \n%s", err, string(out))
			return
		}

		ast = l.LoadFDSet(t, tmpFile)
	})

	return ast
}

// LoadFDSet resolves an AST from a serialized FileDescriptorSet file path on
// l.FS. The test/benchmark is fatally stopped if there is any error.
func (l Loader) LoadFDSet(t T, path string) (ast pgs.AST) {
	fs := l.resolveFS()

	file, err := fs.Open(path)
	if err != nil {
		t.Fatalf("unable to open fdset from path %q: %v", path, err)
		return nil
	}

	defer func() {
		if ferr := file.Close(); ferr != nil {
			t.Logf("unable to close fdset from path %q: %v", path, ferr)
		}
	}()

	return l.LoadFDSetReader(t, file)
}

// LoadFDSetReader resolve an AST from a serialized FileDescriptorSet in r. The
// test/benchmark is fatally stopped if there is any error.
func (l Loader) LoadFDSetReader(t T, r io.Reader) (ast pgs.AST) {
	raw, err := ioutil.ReadAll(r)
	if err != nil {
		t.Fatalf("unable to read fdset: %v", err)
		return nil
	}

	fdset := &descriptor.FileDescriptorSet{}
	if err = proto.Unmarshal(raw, fdset); err != nil {
		t.Fatalf("unable to unmarshal fdset: %v", err)
		return nil
	}

	d := pgs.InitMockDebugger()
	defer func() {
		// Recovery here is required if either Process panics due to how the MockDebugger
		// short circuits the processor (which can currently cause an NPE).
		if err := recover(); err != nil {
			buf, _ := ioutil.ReadAll(d.Output())
			t.Fatalf("failed to process fdset:\n%s", string(buf))
			ast = nil
		}
	}()

	if l.BiDirectional {
		ast = pgs.ProcessFileDescriptorSetBidirectional(d, fdset)
	} else {
		ast = pgs.ProcessFileDescriptorSet(d, fdset)
	}

	if d.Failed() || d.Exited() {
		buf, _ := ioutil.ReadAll(d.Output())
		t.Fatalf("failed to process fdset:\n%s", string(buf))
		return nil
	}

	return ast
}

func (l Loader) resolveFS() afero.Fs {
	if l.FS == nil {
		return afero.NewOsFs()
	}

	return l.FS
}

func (l Loader) resolveProtoc(t T) string {
	if l.Protoc == "" {
		l.Protoc = "protoc"
	}

	path, err := exec.LookPath(l.Protoc)

	if err != nil {
		t.Fatalf("could not find executable protoc: %v", err)
		return l.Protoc
	}

	return path
}

func (l Loader) resolveArgs(tmpFile string, targets []string) []string {
	args := make([]string, 0, 4+len(targets)+2*len(l.ImportPaths))
	args = append(
		args,
		"-o", tmpFile,
		"--include_imports",
		"--include_source_info",
	)

	for _, imp := range l.ImportPaths {
		args = append(args, "-I", imp)
	}

	return append(args, targets...)
}

func (l Loader) resolveTargets(t T, files ...string) []string {
	fs := l.resolveFS()

	targets := make([]string, 0, len(files))
	for _, file := range files {
		matches, err := afero.Glob(fs, file)
		if err != nil {
			t.Fatalf("could not resolve glob %q: %v", file, err)
			return nil
		}
		targets = append(targets, matches...)
	}

	if len(targets) == 0 {
		t.Fatal("no proto files specified")
		return nil
	}

	return targets
}

func (l Loader) withTempDir(t T, fn func(tempDir string)) {
	fs := l.resolveFS()

	tmpDir, err := afero.TempDir(fs, "", "pgs-testutils")
	if err != nil {
		t.Fatalf("could not create temp directory: %v", err)
		return
	}

	defer func() {
		if ferr := fs.RemoveAll(tmpDir); ferr != nil {
			t.Logf("failed to cleanup temp directory: %v", ferr)
		}
	}()

	fn(tmpDir)
}
