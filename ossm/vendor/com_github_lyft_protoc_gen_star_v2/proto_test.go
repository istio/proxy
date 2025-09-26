package pgs

import (
	"testing"

	"github.com/stretchr/testify/assert"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

func TestSyntax_SupportsRequiredPrefix(t *testing.T) {
	t.Parallel()
	assert.True(t, Proto2.SupportsRequiredPrefix())
	assert.False(t, Proto3.SupportsRequiredPrefix())
}

func TestSyntax_String(t *testing.T) {
	t.Parallel()

	assert.Equal(t, Proto2.String(), "")
	assert.Equal(t, Proto3.String(), "proto3")
}

func TestProtoType_IsInt(t *testing.T) {
	t.Parallel()

	yes := []ProtoType{
		Int64T, UInt64T, SFixed64, SInt64, Fixed64T,
		Int32T, UInt32T, SFixed32, SInt32, Fixed32T,
	}

	no := []ProtoType{
		DoubleT, FloatT, BoolT, StringT,
		GroupT, MessageT, BytesT, EnumT,
	}

	for _, pt := range yes {
		assert.True(t, pt.IsInt())
	}

	for _, pt := range no {
		assert.False(t, pt.IsInt())
	}
}

func TestProtoType_IsNumeric(t *testing.T) {
	t.Parallel()

	yes := []ProtoType{
		Int64T, UInt64T, SFixed64, SInt64, Fixed64T,
		Int32T, UInt32T, SFixed32, SInt32, Fixed32T,
		DoubleT, FloatT,
	}

	no := []ProtoType{
		BoolT, StringT, GroupT,
		MessageT, BytesT, EnumT,
	}

	for _, pt := range yes {
		assert.True(t, pt.IsNumeric())
	}

	for _, pt := range no {
		assert.False(t, pt.IsNumeric())
	}
}

func TestProtoType_Proto(t *testing.T) {
	t.Parallel()

	pt := BytesT.Proto()
	ptPtr := BytesT.ProtoPtr()
	assert.Equal(t, descriptor.FieldDescriptorProto_TYPE_BYTES, pt)
	assert.Equal(t, pt, *ptPtr)
}

func TestProtoType_String(t *testing.T) {
	t.Parallel()

	assert.Equal(t, DoubleT.String(), "TYPE_DOUBLE")
}

func TestProtoLabel_Proto(t *testing.T) {
	t.Parallel()

	pl := Repeated.Proto()
	plPtr := Repeated.ProtoPtr()

	assert.Equal(t, descriptor.FieldDescriptorProto_LABEL_REPEATED, pl)
	assert.Equal(t, pl, *plPtr)
}

func TestProtoLabel_String(t *testing.T) {
	t.Parallel()

	assert.Equal(t, Repeated.String(), "LABEL_REPEATED")
}
