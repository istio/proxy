package converter

import (
	"strings"

	"github.com/fatih/camelcase"
	"github.com/iancoleman/strcase"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
)

// Protobuf tag values for relevant message fields. Full list here:
//
//	https://github.com/protocolbuffers/protobuf/blob/master/src/google/protobuf/descriptor.proto
const (
	tag_FileDescriptor_messageType int32 = 4
	tag_FileDescriptor_enumType    int32 = 5
	tag_Descriptor_field           int32 = 2
	tag_Descriptor_nestedType      int32 = 3
	tag_Descriptor_enumType        int32 = 4
	tag_Descriptor_oneofDecl       int32 = 8
	tag_EnumDescriptor_value       int32 = 2
)

type sourceCodeInfo struct {
	lookup map[proto.Message]*descriptor.SourceCodeInfo_Location
}

func (s sourceCodeInfo) GetMessage(m *descriptor.DescriptorProto) *descriptor.SourceCodeInfo_Location {
	return s.lookup[m]
}

func (s sourceCodeInfo) GetField(f *descriptor.FieldDescriptorProto) *descriptor.SourceCodeInfo_Location {
	return s.lookup[f]
}

func (s sourceCodeInfo) GetEnum(e *descriptor.EnumDescriptorProto) *descriptor.SourceCodeInfo_Location {
	return s.lookup[e]
}

func (s sourceCodeInfo) GetEnumValue(e *descriptor.EnumValueDescriptorProto) *descriptor.SourceCodeInfo_Location {
	return s.lookup[e]
}

func newSourceCodeInfo(fs []*descriptor.FileDescriptorProto) *sourceCodeInfo {
	// For each source location in the provided files
	// - resolve the (annoyingly) encoded path to its message/field/service/enum/etc definition
	// - store the source info by its resolved definition
	lookup := map[proto.Message]*descriptor.SourceCodeInfo_Location{}
	for _, f := range fs {
		for _, loc := range f.GetSourceCodeInfo().GetLocation() {
			declaration := getDefinitionAtPath(f, loc.Path)
			if declaration != nil {
				lookup[declaration] = loc
			}
		}
	}
	return &sourceCodeInfo{lookup}
}

// Resolve a protobuf "file-source path" to its associated definition (eg message/field/enum/etc).
// Note that some paths don't point to definitions (some reference subcomponents like name, type,
// field #, etc) and will therefore return nil.
func getDefinitionAtPath(file *descriptor.FileDescriptorProto, path []int32) proto.Message {
	// The way protobuf encodes "file-source path" is a little opaque/tricky;
	// this doc describes how it works:
	//   https://github.com/protocolbuffers/protobuf/blob/master/src/google/protobuf/descriptor.proto#L730

	// Starting at the root of the file descriptor, traverse its object graph by following the
	// specified path (and updating our position/state at each step) until either:
	// - we reach the definition referenced by the path (and return it)
	// - we hit a dead end because the path references a grammar element more granular than a
	//   definition (so we return nil)
	var pos proto.Message = file
	for step := 0; step < len(path); step++ {
		switch p := pos.(type) {
		case *descriptor.FileDescriptorProto:
			switch path[step] {
			case tag_FileDescriptor_messageType:
				step++
				pos = p.MessageType[path[step]]
			case tag_FileDescriptor_enumType:
				step++
				pos = p.EnumType[path[step]]
			default:
				return nil // ignore all other types
			}

		case *descriptor.DescriptorProto:
			switch path[step] {
			case tag_Descriptor_field:
				step++
				pos = p.Field[path[step]]
			case tag_Descriptor_nestedType:
				step++
				pos = p.NestedType[path[step]]
			case tag_Descriptor_enumType:
				step++
				pos = p.EnumType[path[step]]
			case tag_Descriptor_oneofDecl:
				step++
				pos = p.OneofDecl[path[step]]
			default:
				return nil // ignore all other types
			}

		case *descriptor.EnumDescriptorProto:
			switch path[step] {
			case tag_EnumDescriptor_value:
				step++
				pos = p.Value[path[step]]
			default:
				return nil // ignore all other types
			}

		default:
			return nil // ignore all other types
		}
	}
	return pos
}

// formatTitleAndDescription returns a title string and a description string, made from proto comments:
func (c *Converter) formatTitleAndDescription(name *string, sl *descriptor.SourceCodeInfo_Location) (title, description string) {
	var comments []string

	// Default title is camel-cased & split name:
	if name != nil {
		camelName := strcase.ToCamel(*name)
		splitName := camelcase.Split(camelName)
		title = strings.Join(splitName, " ")
	}

	// Leading detached comments first:
	for _, str := range sl.GetLeadingDetachedComments() {
		if s := strings.TrimSpace(str); s != "" {
			comments = append(comments, s)
			title = s
		}
	}

	// Leading comments next:
	if s := strings.TrimSpace(sl.GetLeadingComments()); s != "" {
		comments = append(comments, s)
	}

	// Trailing comments last:
	if s := strings.TrimSpace(sl.GetTrailingComments()); s != "" {
		comments = append(comments, s)
	}

	// The description is all the comments joined together:
	description = strings.Join(comments, c.commentDelimiter)

	// Strip newlines:
	if !c.Flags.KeepNewLinesInDescription {
		description = strings.ReplaceAll(description, "\n", "")
	}

	// Return an empty string if the ExcludeCommentToken is found:
	if strings.Contains(strings.Join(comments, " "), c.excludeCommentToken) {
		return title, ""
	}

	return
}

// Go doesn't have syntax for addressing a string literal, so this is the next best thing.
func strPtr(s string) *string {
	return &s
}
