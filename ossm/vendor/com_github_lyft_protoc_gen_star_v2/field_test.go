package pgs

import (
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

func TestField_Name(t *testing.T) {
	t.Parallel()

	f := &field{desc: &descriptor.FieldDescriptorProto{Name: proto.String("foo")}}

	assert.Equal(t, "foo", f.Name().String())
}

func TestField_FullyQualifiedName(t *testing.T) {
	t.Parallel()

	f := &field{fqn: "field"}
	assert.Equal(t, f.fqn, f.FullyQualifiedName())
}

func TestField_Syntax(t *testing.T) {
	t.Parallel()

	f := &field{}
	m := dummyMsg()
	m.addField(f)

	assert.Equal(t, m.Syntax(), f.Syntax())
}

func TestField_Package(t *testing.T) {
	t.Parallel()

	f := &field{}
	m := dummyMsg()
	m.addField(f)

	assert.NotNil(t, f.Package())
	assert.Equal(t, m.Package(), f.Package())
}

func TestField_File(t *testing.T) {
	t.Parallel()

	f := &field{}
	m := dummyMsg()
	m.addField(f)

	assert.NotNil(t, f.File())
	assert.Equal(t, m.File(), f.File())
}

func TestField_BuildTarget(t *testing.T) {
	t.Parallel()

	f := &field{}
	m := dummyMsg()
	m.addField(f)

	assert.False(t, f.BuildTarget())
	m.setParent(&file{buildTarget: true})
	assert.True(t, f.BuildTarget())
}

func TestField_Descriptor(t *testing.T) {
	t.Parallel()

	f := &field{desc: &descriptor.FieldDescriptorProto{}}
	assert.Equal(t, f.desc, f.Descriptor())
}

func TestField_Message(t *testing.T) {
	t.Parallel()

	f := &field{}
	m := dummyMsg()
	m.addField(f)

	assert.Equal(t, m, f.Message())
}

func TestField_OneOf(t *testing.T) {
	t.Parallel()

	f := &field{}
	assert.Nil(t, f.OneOf())
	assert.False(t, f.InOneOf())

	o := dummyOneof()
	o.addField(f)

	assert.Equal(t, o, f.OneOf())
	assert.True(t, f.InOneOf())
}

func TestField_InRealOneOf(t *testing.T) {
	t.Parallel()

	f := dummyField()
	assert.False(t, f.InRealOneOf())

	f = dummyOneOfField(false)
	assert.True(t, f.InRealOneOf())

	f = dummyOneOfField(true)
	assert.False(t, f.InRealOneOf())
}

func TestField_HasPresence(t *testing.T) {
	t.Parallel()

	f := dummyField()
	f.addType(&repT{scalarT: &scalarT{}})
	assert.False(t, f.HasPresence())

	f.addType(&mapT{repT: &repT{scalarT: &scalarT{}}})
	assert.False(t, f.HasPresence())

	f.addType(&scalarT{})
	assert.False(t, f.HasPresence())

	opt := true
	f.desc = &descriptor.FieldDescriptorProto{Proto3Optional: &opt}
	assert.True(t, f.HasPresence())
}

func TestField_HasOptionalKeyword(t *testing.T) {
	t.Parallel()

	optLabel := descriptor.FieldDescriptorProto_LABEL_OPTIONAL

	f := &field{msg: &msg{parent: dummyFile()}}
	assert.False(t, f.HasOptionalKeyword())

	f.desc = &descriptor.FieldDescriptorProto{Label: &optLabel}
	assert.False(t, f.HasOptionalKeyword())

	f = dummyField()
	assert.False(t, f.HasOptionalKeyword())

	f = dummyOneOfField(false)
	assert.False(t, f.HasOptionalKeyword())

	f = dummyOneOfField(true)
	assert.True(t, f.HasOptionalKeyword())
}

func TestField_Type(t *testing.T) {
	t.Parallel()

	f := &field{}
	f.addType(&scalarT{})

	assert.Equal(t, f.typ, f.Type())
}

func TestField_Extension(t *testing.T) {
	// cannot be parallel

	f := &field{desc: &descriptor.FieldDescriptorProto{}}
	assert.NotPanics(t, func() { f.Extension(nil, nil) })
}

func TestField_Accept(t *testing.T) {
	t.Parallel()

	f := &field{}

	assert.NoError(t, f.accept(nil))

	v := &mockVisitor{err: errors.New("")}
	assert.Error(t, f.accept(v))
	assert.Equal(t, 1, v.field)
}

func TestField_Imports(t *testing.T) {
	t.Parallel()

	f := &field{}
	f.addType(&scalarT{})
	assert.Empty(t, f.Imports())

	f.addType(&mockT{i: []File{&file{}, &file{}}})
	assert.Len(t, f.Imports(), 2)
}

func TestField_Required(t *testing.T) {
	t.Parallel()

	msg := dummyMsg()

	lbl := descriptor.FieldDescriptorProto_LABEL_REQUIRED

	f := &field{desc: &descriptor.FieldDescriptorProto{Label: &lbl}}
	f.setMessage(msg)

	assert.False(t, f.Required(), "proto3 messages can never be marked required")

	f.File().(*file).desc.Syntax = proto.String(string(Proto2))
	assert.True(t, f.Required(), "proto2 + required")

	lbl = descriptor.FieldDescriptorProto_LABEL_OPTIONAL
	f.desc.Label = &lbl
	assert.False(t, f.Required(), "proto2 + optional")
}

func TestField_ChildAtPath(t *testing.T) {
	t.Parallel()

	f := &field{}
	assert.Equal(t, f, f.childAtPath(nil))
	assert.Nil(t, f.childAtPath([]int32{1}))
}

type mockField struct {
	Field
	i   []File
	m   Message
	err error
}

func (f *mockField) Imports() []File { return f.i }

func (f *mockField) setMessage(m Message) { f.m = m }

func (f *mockField) accept(v Visitor) error {
	_, err := v.VisitField(f)
	if f.err != nil {
		return f.err
	}
	return err
}

func dummyField() *field {
	m := dummyMsg()
	str := descriptor.FieldDescriptorProto_TYPE_STRING
	f := &field{desc: &descriptor.FieldDescriptorProto{Name: proto.String("field"), Type: &str}}
	m.addField(f)
	t := &scalarT{}
	f.addType(t)
	return f
}

func dummyOneOfField(synthetic bool) *field {
	m := dummyMsg()
	o := dummyOneof()
	str := descriptor.FieldDescriptorProto_TYPE_STRING
	var oIndex int32 = 1
	f := &field{desc: &descriptor.FieldDescriptorProto{
		Name:           proto.String("field"),
		Type:           &str,
		OneofIndex:     &oIndex,
		Proto3Optional: &synthetic,
	}}
	o.addField(f)
	m.addField(f)
	m.addOneOf(o)
	t := &scalarT{}
	f.addType(t)
	return f
}
