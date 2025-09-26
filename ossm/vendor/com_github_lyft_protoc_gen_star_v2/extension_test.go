package pgs

import (
	"bytes"
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/runtime/protoimpl"
)

func TestExt_FullyQualifiedName(t *testing.T) {
	t.Parallel()

	e := &ext{fqn: "foo"}
	assert.Equal(t, e.fqn, e.FullyQualifiedName())
}

func TestExt_Syntax(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()
	e := &ext{parent: msg}
	assert.Equal(t, msg.Syntax(), e.Syntax())
}

func TestExt_Package(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()
	e := &ext{parent: msg}
	assert.Equal(t, msg.Package(), e.Package())
}

func TestExt_File(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()
	e := &ext{parent: msg}
	assert.Equal(t, msg.File(), e.File())
}

func TestExt_BuildTarget(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()
	e := &ext{parent: msg}
	assert.Equal(t, msg.BuildTarget(), e.BuildTarget())
}

func TestExt_ParentEntity(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()
	e := &ext{parent: msg}
	assert.Equal(t, msg, e.DefinedIn())
}

func TestExt_Extendee(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()
	e := &ext{}
	e.setExtendee(msg)
	assert.Equal(t, msg, e.Extendee())
}

func TestExt_Message(t *testing.T) {
	t.Parallel()

	e := &ext{}
	assert.Nil(t, e.Message())
}

func TestExt_InOneOf(t *testing.T) {
	t.Parallel()

	e := &ext{}
	assert.False(t, e.InOneOf())
}

func TestExt_OneOf(t *testing.T) {
	t.Parallel()

	e := &ext{}
	assert.Nil(t, e.OneOf())
}

func TestExt_Accept(t *testing.T) {
	t.Parallel()

	e := &ext{}

	assert.NoError(t, e.accept(nil))

	v := &mockVisitor{err: errors.New("")}
	assert.Error(t, e.accept(v))
	assert.Equal(t, 1, v.extension)
}

type mockExtractor struct {
	has bool
	get interface{}
}

func (e *mockExtractor) HasExtension(proto.Message, *protoimpl.ExtensionInfo) bool { return e.has }

func (e *mockExtractor) GetExtension(proto.Message, *protoimpl.ExtensionInfo) interface{} {
	return e.get
}

var testExtractor = &mockExtractor{}

func init() { extractor = testExtractor }

func TestExtension(t *testing.T) {
	// cannot be parallel
	defer func() { testExtractor.get = nil }()

	found, err := extension(nil, nil, nil)
	assert.False(t, found)
	assert.NoError(t, err)

	found, err = extension(proto.Message(nil), nil, nil)
	assert.False(t, found)
	assert.NoError(t, err)

	opts := &struct{ proto.Message }{}
	found, err = extension(opts, nil, nil)
	assert.False(t, found)
	assert.EqualError(t, err, "nil *protoimpl.ExtensionInfo parameter provided")

	desc := &protoimpl.ExtensionInfo{}
	found, err = extension(opts, desc, nil)
	assert.False(t, found)
	assert.EqualError(t, err, "nil extension output parameter provided")

	type myExt struct{ Name string }
	found, err = extension(opts, desc, &myExt{})
	assert.False(t, found)
	assert.NoError(t, err)
	testExtractor.has = true

	found, err = extension(opts, desc, &myExt{})
	assert.False(t, found)
	assert.EqualError(t, err, "extracted extension value is nil")

	testExtractor.get = &myExt{"bar"}
	out := myExt{}
	found, err = extension(opts, desc, out)
	assert.False(t, found)
	assert.EqualError(t, err, "out parameter must be a pointer type")

	found, err = extension(opts, desc, &out)
	assert.True(t, found)
	assert.NoError(t, err)
	assert.Equal(t, "bar", out.Name)

	var ref *myExt
	found, err = extension(opts, desc, &ref)
	assert.True(t, found)
	assert.NoError(t, err)
	assert.Equal(t, "bar", ref.Name)

	found, err = extension(opts, desc, &bytes.Buffer{})
	assert.True(t, found)
	assert.Error(t, err)
}

func TestProtoExtExtractor(t *testing.T) {
	e := protoExtExtractor{}
	assert.NotPanics(t, func() { e.HasExtension(nil, nil) })
	assert.Panics(t, func() { e.GetExtension(nil, nil) })
}

// needed to wrapped since there is a Extension method
type mExt interface {
	Extension
}

type mockExtension struct {
	mExt
	err error
}

func (e *mockExtension) accept(v Visitor) error {
	_, err := v.VisitExtension(e)
	if e.err != nil {
		return e.err
	}
	return err
}
