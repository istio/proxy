package main

import (
	plugin "github.com/gogo/protobuf/protoc-gen-gogo/plugin"
	"github.com/gogo/protobuf/vanity"
	"github.com/gogo/protobuf/vanity/command"
)

func main() {
	req := command.Read()
	files := req.GetProtoFile()
	files = vanity.FilterFiles(files, vanity.NotGoogleProtobufDescriptorProto)

	vanity.ForEachFile(files, vanity.TurnOffGoStringerAll)
	vanity.ForEachFile(files, vanity.TurnOffGoEnumStringerAll)

	resp := command.Generate(req)
	command.Write(resp)

	baseFiles := req.FileToGenerate

	dbenumGenerator := NewGenerator()
	req = onlyEnumFiles(req, baseFiles)
	if len(req.FileToGenerate) > 0 {
		resp = command.GeneratePlugin(req, dbenumGenerator, "_dbenum.pb.go")
		command.Write(resp)
	}
}

func onlyEnumFiles(
	req *plugin.CodeGeneratorRequest, baseFiles []string,
) *plugin.CodeGeneratorRequest {
	// Find out files that contains enum value with dbenum extension.
	dbEnumFiles := make(map[string]bool)
	for _, file := range req.GetProtoFile() {
		for _, enum := range file.EnumType {
			if HasDBEnum(enum.Value) {
				dbEnumFiles[*file.Name] = true
				break
			}
		}
	}
	enumFilesToGenerate := make([]string, 0, len(baseFiles))
	for _, file := range baseFiles {
		if dbEnumFiles[file] {
			enumFilesToGenerate = append(enumFilesToGenerate, file)
		}
	}
	req.FileToGenerate = enumFilesToGenerate
	return req
}
