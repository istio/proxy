package pgsgo

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"

	pgs "github.com/lyft/protoc-gen-star/v2"
)

func TestTypeName(t *testing.T) {
	t.Parallel()

	tests := []struct {
		in  string
		el  string
		key string
		ptr string
		val string
	}{
		{
			in:  "int",
			el:  "int",
			ptr: "*int",
			val: "int",
		},
		{
			in:  "*int",
			el:  "*int",
			ptr: "*int",
			val: "int",
		},
		{
			in:  "foo.bar",
			el:  "foo.bar",
			ptr: "*foo.bar",
			val: "foo.bar",
		},
		{
			in:  "*foo.bar",
			el:  "*foo.bar",
			ptr: "*foo.bar",
			val: "foo.bar",
		},
		{
			in:  "[]string",
			el:  "string",
			key: "int",
			ptr: "[]string",
			val: "[]string",
		},
		{
			in:  "[]*string",
			el:  "*string",
			key: "int",
			ptr: "[]*string",
			val: "[]*string",
		},
		{
			in:  "[]foo.bar",
			el:  "foo.bar",
			key: "int",
			ptr: "[]foo.bar",
			val: "[]foo.bar",
		},
		{
			in:  "[]*foo.bar",
			el:  "*foo.bar",
			key: "int",
			ptr: "[]*foo.bar",
			val: "[]*foo.bar",
		},
		{
			in:  "map[string]float64",
			el:  "float64",
			key: "string",
			ptr: "map[string]float64",
			val: "map[string]float64",
		},
		{
			in:  "map[string]*float64",
			el:  "*float64",
			key: "string",
			ptr: "map[string]*float64",
			val: "map[string]*float64",
		},
		{
			in:  "map[string]foo.bar",
			el:  "foo.bar",
			key: "string",
			ptr: "map[string]foo.bar",
			val: "map[string]foo.bar",
		},
		{
			in:  "map[string]*foo.bar",
			el:  "*foo.bar",
			key: "string",
			ptr: "map[string]*foo.bar",
			val: "map[string]*foo.bar",
		},
		{
			in:  "[][]byte",
			el:  "[]byte",
			key: "int",
			ptr: "[][]byte",
			val: "[][]byte",
		},
		{
			in:  "map[int64][]byte",
			el:  "[]byte",
			key: "int64",
			ptr: "map[int64][]byte",
			val: "map[int64][]byte",
		},
	}

	for _, test := range tests {
		tc := test
		t.Run(tc.in, func(t *testing.T) {
			tn := TypeName(tc.in)
			t.Parallel()

			t.Run("Element", func(t *testing.T) {
				t.Parallel()
				assert.Equal(t, tc.el, tn.Element().String())
			})

			t.Run("Key", func(t *testing.T) {
				t.Parallel()
				assert.Equal(t, tc.key, tn.Key().String())
			})

			t.Run("IsPointer", func(t *testing.T) {
				t.Parallel()
				assert.Equal(t, tc.ptr == tc.in, tn.IsPointer())
			})

			t.Run("Pointer", func(t *testing.T) {
				t.Parallel()
				assert.Equal(t, tc.ptr, tn.Pointer().String())
			})

			t.Run("Value", func(t *testing.T) {
				t.Parallel()
				assert.Equal(t, tc.val, tn.Value().String())
			})
		})
	}
}

func TestTypeName_Key_Malformed(t *testing.T) {
	t.Parallel()
	tn := TypeName("]malformed")
	assert.Empty(t, tn.Key().String())
}

func TestScalarType_Invalid(t *testing.T) {
	t.Parallel()
	assert.Panics(t, func() {
		scalarType(pgs.ProtoType(0))
	})
}

func ExampleTypeName_Element() {
	types := []string{
		"int",
		"*my.Type",
		"[]string",
		"map[string]*io.Reader",
	}

	for _, t := range types {
		fmt.Println(TypeName(t).Element())
	}

	// Output:
	// int
	// *my.Type
	// string
	// *io.Reader
}

func ExampleTypeName_Key() {
	types := []string{
		"int",
		"*my.Type",
		"[]string",
		"map[string]*io.Reader",
	}

	for _, t := range types {
		fmt.Println(TypeName(t).Key())
	}

	// Output:
	//
	//
	// int
	// string
}

func ExampleTypeName_IsPointer() {
	types := []string{
		"int",
		"*my.Type",
		"[]string",
		"map[string]*io.Reader",
	}

	for _, t := range types {
		fmt.Println(TypeName(t).IsPointer())
	}

	// Output:
	// false
	// true
	// true
	// true
}

func ExampleTypeName_Pointer() {
	types := []string{
		"int",
		"*my.Type",
		"[]string",
		"map[string]*io.Reader",
	}

	for _, t := range types {
		fmt.Println(TypeName(t).Pointer())
	}

	// Output:
	// *int
	// *my.Type
	// []string
	// map[string]*io.Reader
}

func ExampleTypeName_Value() {
	types := []string{
		"int",
		"*my.Type",
		"[]string",
		"map[string]*io.Reader",
	}

	for _, t := range types {
		fmt.Println(TypeName(t).Value())
	}

	// Output:
	// int
	// my.Type
	// []string
	// map[string]*io.Reader
}
