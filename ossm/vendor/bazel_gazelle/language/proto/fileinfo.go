/* Copyright 2018 The Bazel Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package proto

import (
	"bytes"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

// FileInfo contains metadata extracted from a .proto file.
type FileInfo struct {
	Path, Name string

	PackageName string

	Options []Option
	Imports []string

	HasServices bool

	Services []string
	Messages []string
	Enums []string
}

// Option represents a top-level option statement in a .proto file. Only
// string options are supported for now.
type Option struct {
	Key, Value string
}

var protoRe = buildProtoRegexp()

func ProtoFileInfo(dir, name string) FileInfo {
	info := FileInfo{
		Path: filepath.Join(dir, name),
		Name: name,
	}
	content, err := os.ReadFile(info.Path)
	if err != nil {
		log.Printf("%s: error reading proto file: %v", info.Path, err)
		return info
	}

	for _, match := range protoRe.FindAllSubmatch(content, -1) {
		switch {
		case match[importSubexpIndex] != nil:
			imp := unquoteProtoString(match[importSubexpIndex])
			info.Imports = append(info.Imports, imp)

		case match[packageSubexpIndex] != nil:
			pkg := string(match[packageSubexpIndex])
			if info.PackageName == "" {
				info.PackageName = pkg
			}

		case match[optkeySubexpIndex] != nil:
			key := string(match[optkeySubexpIndex])
			value := unquoteProtoString(match[optvalSubexpIndex])
			info.Options = append(info.Options, Option{key, value})

		case match[serviceSubexpIndex] != nil:
			info.HasServices = true

			// match is of the format "service ServiceName {".
			// extract just the service name
			fullMatch := string(match[serviceSubexpIndex])
			if serviceName, ok := extractObjectName(fullMatch); ok {
				info.Services = append(info.Services, serviceName)
			}

		case match[messageSubexpIndex] != nil:
			// match is of the format "message MessageName {".
			// extract just the message name
			fullMatch := string(match[messageSubexpIndex])
			if messageName, ok := extractObjectName(fullMatch); ok {
				info.Messages = append(info.Messages, messageName)
			}



		case match[enumSubexpIndex] != nil:
			// match is of the format "enum EnumName {".
			// extract just the enum name
			fullMatch := string(match[enumSubexpIndex])
			if enumName, ok := extractObjectName(fullMatch); ok {
				info.Enums = append(info.Enums, enumName)
			}


		default:
			// Comment matched. Nothing to extract.
		}
	}
	sort.Strings(info.Imports)

	return info
}

const (
	importSubexpIndex  = 1
	packageSubexpIndex = 2
	optkeySubexpIndex  = 3
	optvalSubexpIndex  = 4
	serviceSubexpIndex = 5
	messageSubexpIndex = 6
	enumSubexpIndex = 7
)

// Based on https://developers.google.com/protocol-buffers/docs/reference/proto3-spec
func buildProtoRegexp() *regexp.Regexp {
	hexEscape := `\\[xX][0-9a-fA-f]{2}`
	octEscape := `\\[0-7]{3}`
	charEscape := `\\[abfnrtv'"\\]`
	charValue := strings.Join([]string{hexEscape, octEscape, charEscape, "[^\x00\\'\\\"\\\\]"}, "|")
	strLit := `'(?:` + charValue + `|")*'|"(?:` + charValue + `|')*"`
	ident := `[A-Za-z][A-Za-z0-9_]*`
	fullIdent := ident + `(?:\.` + ident + `)*`
	importStmt := `\bimport\s*(?:public|weak)?\s*(?P<import>` + strLit + `)\s*;`
	packageStmt := `\bpackage\s*(?P<package>` + fullIdent + `)\s*;`
	optionStmt := `\boption\s*(?P<optkey>` + fullIdent + `)\s*=\s*(?P<optval>` + strLit + `)\s*;`
	serviceStmt := `(?P<service>service\s+` + ident + `\s*{)`
	messageStmt := `(?P<message>message\s+` + ident + `\s*{)`
	enumStmt := `(?P<enum>enum\s+` + ident + `\s*{)`
	comment := `//[^\n]*`
	protoReSrc := strings.Join([]string{importStmt, packageStmt, optionStmt, serviceStmt, messageStmt, enumStmt, comment}, "|")
	return regexp.MustCompile(protoReSrc)
}

func unquoteProtoString(q []byte) string {
	// Adjust quotes so that Unquote is happy. We need a double quoted string
	// without unescaped double quote characters inside.
	noQuotes := bytes.Split(q[1:len(q)-1], []byte{'"'})
	if len(noQuotes) != 1 {
		for i := 0; i < len(noQuotes)-1; i++ {
			if len(noQuotes[i]) == 0 || noQuotes[i][len(noQuotes[i])-1] != '\\' {
				noQuotes[i] = append(noQuotes[i], '\\')
			}
		}
		q = append([]byte{'"'}, bytes.Join(noQuotes, []byte{'"'})...)
		q = append(q, '"')
	}
	if q[0] == '\'' {
		q[0] = '"'
		q[len(q)-1] = '"'
	}

	s, err := strconv.Unquote(string(q))
	if err != nil {
		log.Panicf("unquoting string literal %s from proto: %v", q, err)
	}
	return s
}

func extractObjectName(fullMatch string) (response string, ok bool) {
	fields := strings.Fields(fullMatch)
	if len(fields) < 2 {
	  // expect as least two fields. Input is malformed
	  return "", false
	}

	return strings.TrimSuffix(fields[1], "{"), true
  }
