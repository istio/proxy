package main

import (
	"bytes"
	"strings"
	"text/template"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/protoc-gen-gogo/descriptor"
	pb "github.com/gogo/protobuf/protoc-gen-gogo/descriptor"
	"github.com/gogo/protobuf/protoc-gen-gogo/generator"
)

func init() {
	generator.RegisterPlugin(NewGenerator())
}

type Generator struct {
	*generator.Generator
	generator.PluginImports
	write bool
}

func NewGenerator() *Generator {
	return &Generator{}
}

func (g *Generator) Name() string {
	return "dbenum"
}

func (g *Generator) Init(gen *generator.Generator) {
	g.Generator = gen
}

func (g *Generator) GenerateImports(file *generator.FileDescriptor) {
}

func (g *Generator) Generate(file *generator.FileDescriptor) {
	for _, enum := range file.Enums() {
		g.enumHelper(enum)
	}
	g.writeTrailer(file.Enums())
}

func (g *Generator) Write() bool {
	return g.write
}

const initTmpl = `
`

func (g *Generator) writeTrailer(enums []*generator.EnumDescriptor) {
	type desc struct {
		PackageName       string
		TypeName          string
		LowerCaseTypeName string
	}
	if !g.write {
		return
	}
	tmpl := template.Must(template.New("db_enum_trailer").Parse(initTmpl))
	g.P("func init() {")
	for _, e := range enums {
		if !HasDBEnum(e.Value) {
			continue
		}
		pkg := e.File().GetPackage()
		if pkg != "" {
			pkg += "."
		}
		tp := generator.CamelCaseSlice(e.TypeName())
		var buf bytes.Buffer
		tmpl.Execute(&buf, desc{
			PackageName:       pkg + tp,
			TypeName:          tp,
			LowerCaseTypeName: strings.ToLower(tp),
		})
		g.P(buf.String())
	}
	g.P("}")
}

func (g *Generator) enumHelper(enum *generator.EnumDescriptor) {
	type anEnum struct {
		PBName string
		DBName string
	}
	type typeDesc struct {
		TypeName          string
		TypeNamespace     string
		LowerCaseTypeName string
		Found             map[int32]bool
		Names             []anEnum
		AllNames          []anEnum
	}
	tp := generator.CamelCaseSlice(enum.TypeName())
	namespace := tp
	enumTypeName := enum.TypeName()
	if len(enumTypeName) > 1 { // This is a nested enum.
		names := enumTypeName[:len(enumTypeName)-1]
		// See https://protobuf.dev/reference/go/go-generated/#enum
		namespace = generator.CamelCaseSlice(names)
	}
	t := typeDesc{
		TypeName:          tp,
		TypeNamespace:     namespace,
		LowerCaseTypeName: strings.ToLower(tp),
		Found:             make(map[int32]bool),
	}
	for _, v := range enum.Value {
		enumValue := v.GetNumber()
		if validDbEnum, dbName := getDbEnum(v); validDbEnum {
			names := anEnum{PBName: v.GetName(), DBName: dbName}
			t.AllNames = append(t.AllNames, names)
			// Skip enums that are aliased where one value has already been processed.
			if t.Found[enumValue] {
				continue
			}
			t.Found[enumValue] = true
			t.Names = append(t.Names, names)
		} else {
			t.Found[enumValue] = true
		}
	}
	if len(t.AllNames) == 0 {
		return
	}
	g.write = true
	tmpl := template.Must(template.New("db_enum").Parse(tmpl))
	var buf bytes.Buffer
	tmpl.Execute(&buf, t)
	g.P(buf.String())
}

var E_DbEnum = &proto.ExtensionDesc{
	ExtendedType:  (*descriptor.EnumValueOptions)(nil),
	ExtensionType: (*string)(nil),
	Field:         5002,
	Name:          "tests.core.go_proto_library.enum",
	Tag:           "bytes,5002,opt,name=db_enum",
}

func getDbEnum(value *pb.EnumValueDescriptorProto) (bool, string) {
	if value == nil || value.Options == nil {
		return false, ""
	}
	EDbEnum := E_DbEnum
	v, err := proto.GetExtension(value.Options, EDbEnum)
	if err != nil {
		return false, ""
	}
	strPtr := v.(*string)
	if strPtr == nil {
		return false, ""
	}
	return true, *strPtr
}

// HasDBEnum returns if there is DBEnums extensions defined in given enums.
func HasDBEnum(enums []*pb.EnumValueDescriptorProto) bool {
	for _, enum := range enums {
		if validDbEnum, _ := getDbEnum(enum); validDbEnum {
			return true
		}
	}
	return false
}

const tmpl = `

var {{ .LowerCaseTypeName }}ToStringValue = ` +
	`map[{{ .TypeName }}]string { {{ range $names := .Names }}
    {{ $.TypeNamespace }}_{{ $names.PBName }}: ` +
	`"{{ $names.DBName }}",{{ end }}
}


// String implements the stringer interface and should produce the same output
// that is inserted into the db.
func (v {{ .TypeName }}) String() string {
	if val, ok := {{ .LowerCaseTypeName }}ToStringValue[v]; ok {
		return val
	} else if int(v) == 0 {
		return "null"
	} else {
		return proto.EnumName({{ .TypeName }}_name, int32(v))
	}
}`
