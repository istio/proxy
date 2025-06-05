// +build proto3_presence

package pgsgo

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	pgs "github.com/lyft/protoc-gen-star/v2"
)

func TestType(t *testing.T) {
	t.Parallel()

	ast := buildGraph(t, "presence", "types")
	ctx := loadContext(t, "presence", "types")

	tests := []struct {
		field    string
		expected TypeName
	}{
		{"Proto3.double", "float64"},
		{"Proto3.float", "float32"},
		{"Proto3.int64", "int64"},
		{"Proto3.sfixed64", "int64"},
		{"Proto3.sint64", "int64"},
		{"Proto3.uint64", "uint64"},
		{"Proto3.fixed64", "uint64"},
		{"Proto3.int32", "int32"},
		{"Proto3.sfixed32", "int32"},
		{"Proto3.sint32", "int32"},
		{"Proto3.uint32", "uint32"},
		{"Proto3.fixed32", "uint32"},
		{"Proto3.bool", "bool"},
		{"Proto3.string", "string"},
		{"Proto3.bytes", "[]byte"},
		{"Proto3.enum", "Proto3_Enum"},
		{"Proto3.ext_enum", "typepb.Syntax"},
		{"Proto3.msg", "*Proto3_Message"},
		{"Proto3.ext_msg", "*durationpb.Duration"},
		{"Proto3.repeated_scalar", "[]float64"},
		{"Proto3.repeated_enum", "[]Proto3_Enum"},
		{"Proto3.repeated_ext_enum", "[]typepb.Syntax"},
		{"Proto3.repeated_msg", "[]*Proto3_Message"},
		{"Proto3.repeated_ext_msg", "[]*durationpb.Duration"},
		{"Proto3.map_scalar", "map[string]float32"},
		{"Proto3.map_enum", "map[int32]Proto3_Enum"},
		{"Proto3.map_ext_enum", "map[uint64]typepb.Syntax"},
		{"Proto3.map_msg", "map[uint32]*Proto3_Message"},
		{"Proto3.map_ext_msg", "map[int64]*durationpb.Duration"},

		// proto3 syntax optional
		{"Proto3.Optional.double", "*float64"},
		{"Proto3.Optional.float", "*float32"},
		{"Proto3.Optional.int64", "*int64"},
		{"Proto3.Optional.sfixed64", "*int64"},
		{"Proto3.Optional.sint64", "*int64"},
		{"Proto3.Optional.uint64", "*uint64"},
		{"Proto3.Optional.fixed64", "*uint64"},
		{"Proto3.Optional.int32", "*int32"},
		{"Proto3.Optional.sfixed32", "*int32"},
		{"Proto3.Optional.sint32", "*int32"},
		{"Proto3.Optional.uint32", "*uint32"},
		{"Proto3.Optional.fixed32", "*uint32"},
		{"Proto3.Optional.bool", "*bool"},
		{"Proto3.Optional.string", "*string"},
		{"Proto3.Optional.bytes", "[]byte"},
		{"Proto3.Optional.enum", "*Proto3_Enum"},
		{"Proto3.Optional.ext_enum", "*typepb.Syntax"},
		{"Proto3.Optional.msg", "*Proto3_Optional"},
		{"Proto3.Optional.ext_msg", "*durationpb.Duration"},
	}

	for _, test := range tests {
		tc := test
		t.Run(tc.field, func(t *testing.T) {
			t.Parallel()

			e, ok := ast.Lookup(".names.types." + tc.field)
			require.True(t, ok, "could not find field")

			fld, ok := e.(pgs.Field)
			require.True(t, ok, "entity is not a field")

			assert.Equal(t, tc.expected, ctx.Type(fld))
		})
	}
}
