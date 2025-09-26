package converter

import (
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"path"
	"regexp"
	"strings"

	"github.com/alecthomas/jsonschema"
	"github.com/iancoleman/strcase"
	"github.com/sirupsen/logrus"
	"github.com/xeipuuv/gojsonschema"
	gengo "google.golang.org/protobuf/cmd/protoc-gen-go/internal_gengo"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"
	plugin "google.golang.org/protobuf/types/pluginpb"

	protoc_gen_jsonschema "github.com/chrusty/protoc-gen-jsonschema"
)

const (
	defaultCommentDelimiter    = "  "
	defaultExcludeCommentToken = "@exclude"
	defaultFileExtension       = "json"
	defaultPackageName         = "package"
	defaultRefPrefix           = "#/definitions/"
	messageDelimiter           = "+"
	versionDraft04             = "http://json-schema.org/draft-04/schema#"
	versionDraft06             = "http://json-schema.org/draft-06/schema#"
)

// Converter is everything you need to convert protos to JSONSchemas:
type Converter struct {
	Flags               ConverterFlags
	commentDelimiter    string
	excludeCommentToken string
	logger              *logrus.Logger
	refPrefix           string
	schemaFileExtension string
	schemaVersion       string
	sourceInfo          *sourceCodeInfo
	messageTargets      []string
}

// ConverterFlags control the behaviour of the converter:
type ConverterFlags struct {
	AllFieldsRequired            bool
	AllowNullValues              bool
	DisallowAdditionalProperties bool
	DisallowBigIntsAsStrings     bool
	EnforceOneOf                 bool
	EnumsAsConstants             bool
	EnumsAsStringsOnly           bool
	EnumsTrimPrefix              bool
	KeepNewLinesInDescription    bool
	PrefixSchemaFilesWithPackage bool
	UseJSONFieldnamesOnly        bool
	UseProtoAndJSONFieldNames    bool
}

// New returns a configured *Converter (defaulting to draft-04 version):
func New(logger *logrus.Logger) *Converter {
	return &Converter{
		commentDelimiter:    defaultCommentDelimiter,
		excludeCommentToken: defaultExcludeCommentToken,
		logger:              logger,
		refPrefix:           defaultRefPrefix,
		schemaFileExtension: defaultFileExtension,
		schemaVersion:       versionDraft04,
	}
}

// ConvertFrom tells the convert to work on the given input:
func (c *Converter) ConvertFrom(rd io.Reader) (*plugin.CodeGeneratorResponse, error) {
	c.logger.Debug("Reading code generation request")
	input, err := ioutil.ReadAll(rd)
	if err != nil {
		c.logger.WithError(err).Error("Failed to read request")
		return nil, err
	}

	req := &plugin.CodeGeneratorRequest{}
	err = proto.Unmarshal(input, req)
	if err != nil {
		c.logger.WithError(err).Error("Can't unmarshal input")
		return nil, err
	}

	c.logger.Debug("Converting input")
	return c.convert(req)
}

func (c *Converter) parseGeneratorParameters(parameters string) {
	for _, parameter := range strings.Split(parameters, ",") {
		switch parameter {
		case "all_fields_required":
			c.Flags.AllFieldsRequired = true
		case "allow_null_values":
			c.Flags.AllowNullValues = true
		case "debug":
			c.logger.SetLevel(logrus.DebugLevel)
		case "disallow_additional_properties":
			c.Flags.DisallowAdditionalProperties = true
		case "disallow_bigints_as_strings":
			c.Flags.DisallowBigIntsAsStrings = true
		case "enforce_oneof":
			c.Flags.EnforceOneOf = true
		case "enums_as_strings_only":
			c.Flags.EnumsAsStringsOnly = true
		case "enums_trim_prefix":
			c.Flags.EnumsTrimPrefix = true
		case "json_fieldnames":
			c.Flags.UseJSONFieldnamesOnly = true
		case "prefix_schema_files_with_package":
			c.Flags.PrefixSchemaFilesWithPackage = true
		case "proto_and_json_fieldnames":
			c.Flags.UseProtoAndJSONFieldNames = true
		}

		// look for specific message targets
		// message types are separated by messageDelimiter "+"
		// examples:
		// 		messages=[foo+bar]
		// 		messages=[foo]
		rx := regexp.MustCompile(`messages=\[([^\]]+)\]`)
		if matches := rx.FindStringSubmatch(parameter); len(matches) == 2 {
			c.messageTargets = strings.Split(matches[1], messageDelimiter)
		}

		// Configure custom file extension:
		if parameterParts := strings.Split(parameter, "file_extension="); len(parameterParts) == 2 {
			c.schemaFileExtension = parameterParts[1]
		}
	}
}

// Converts a proto "ENUM" into a JSON-Schema:
func (c *Converter) convertEnumType(enum *descriptor.EnumDescriptorProto, converterFlags ConverterFlags) (jsonschema.Type, error) {

	// Prepare a new jsonschema.Type for our eventual return value:
	jsonSchemaType := jsonschema.Type{}

	// Inherit the CLI converterFlags:
	converterFlags.EnumsAsStringsOnly = c.Flags.EnumsAsStringsOnly

	// Set some per-enum flags from config and options:
	if opts := enum.GetOptions(); opts != nil && proto.HasExtension(opts, protoc_gen_jsonschema.E_EnumOptions) {
		if opt := proto.GetExtension(opts, protoc_gen_jsonschema.E_EnumOptions); opt != nil {
			if enumOptions, ok := opt.(*protoc_gen_jsonschema.EnumOptions); ok {

				// ENUMs as constants:
				if enumOptions.GetEnumsAsConstants() {
					converterFlags.EnumsAsConstants = true
				}

				// ENUM values as strings only:
				if enumOptions.GetEnumsAsStringsOnly() {
					converterFlags.EnumsAsStringsOnly = true
				}

				// ENUM values trim enum name prefix:
				if enumOptions.GetEnumsTrimPrefix() {
					converterFlags.EnumsTrimPrefix = true
				}

				// If this particular ENUM is marked with the "ignore" option then return a skipped error:
				if enumOptions.GetIgnore() {
					c.logger.WithField("msg_name", enum.GetName()).Debug("Skipping ignored enum")
					return jsonSchemaType, errIgnored
				}
			}
		}
	}

	// Generate a description from src comments (if available):
	if src := c.sourceInfo.GetEnum(enum); src != nil {
		jsonSchemaType.Title, jsonSchemaType.Description = c.formatTitleAndDescription(strPtr(enum.GetName()), src)
	}

	// Use basic types if we're not opting to use constants for ENUMs:
	if !converterFlags.EnumsAsConstants {
		jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Type: gojsonschema.TYPE_STRING})
		if !converterFlags.EnumsAsStringsOnly {
			jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Type: gojsonschema.TYPE_INTEGER})
		}
	}

	// Optionally allow NULL values:
	if converterFlags.AllowNullValues {
		jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Type: gojsonschema.TYPE_NULL})
	}

	// If we end up with just one option in OneOf, unwrap it
	if len(jsonSchemaType.OneOf) == 1 {
		jsonSchemaType.Type = jsonSchemaType.OneOf[0].Type
		jsonSchemaType.OneOf = nil
	}

	// If we need to trim prefix from enum value
	enumNamePrefix := fmt.Sprintf("%s_", strcase.ToScreamingSnake(*enum.Name))

	// We have found an enum, append its values:
	for _, value := range enum.Value {

		// Each ENUM value can have comments too:
		var valueDescription string
		if src := c.sourceInfo.GetEnumValue(value); src != nil {
			_, valueDescription = c.formatTitleAndDescription(nil, src)
		}

		valueName := value.GetName()

		// If enum name prefix should be removed from enum value name:
		if converterFlags.EnumsTrimPrefix {
			valueName = strings.TrimPrefix(valueName, enumNamePrefix)
		}

		// If we're using constants for ENUMs then add these here, along with their title:
		if converterFlags.EnumsAsConstants {
			c.schemaVersion = versionDraft06 // Const requires draft-06
			jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Extras: map[string]interface{}{"const": valueName}, Description: valueDescription})
			if !converterFlags.EnumsAsStringsOnly {
				jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Extras: map[string]interface{}{"const": value.GetNumber()}, Description: valueDescription})
			}
		}

		// Add the values to the ENUM:
		jsonSchemaType.Enum = append(jsonSchemaType.Enum, valueName)
		if !converterFlags.EnumsAsStringsOnly {
			jsonSchemaType.Enum = append(jsonSchemaType.Enum, value.Number)
		}
	}

	return jsonSchemaType, nil
}

// Converts a proto file into a JSON-Schema:
func (c *Converter) convertFile(file *descriptor.FileDescriptorProto, fileExtension string) ([]*plugin.CodeGeneratorResponse_File, error) {

	// Input filename:
	protoFileName := path.Base(file.GetName())

	// Prepare a list of responses:
	var response []*plugin.CodeGeneratorResponse_File

	// user wants specific messages
	genSpecificMessages := len(c.messageTargets) > 0

	// Warn about multiple messages / enums in files:
	if !genSpecificMessages && len(file.GetMessageType()) > 1 {
		c.logger.WithField("schemas", len(file.GetMessageType())).WithField("proto_filename", protoFileName).Debug("protoc-gen-jsonschema will create multiple MESSAGE schemas from one proto file")
	}

	if len(file.GetEnumType()) > 1 {
		c.logger.WithField("schemas", len(file.GetMessageType())).WithField("proto_filename", protoFileName).Debug("protoc-gen-jsonschema will create multiple ENUM schemas from one proto file")
	}

	// Generate standalone ENUMs:
	if len(file.GetMessageType()) == 0 {
		for _, enum := range file.GetEnumType() {
			jsonSchemaFileName := c.generateSchemaFilename(file, fileExtension, enum.GetName())
			c.logger.WithField("proto_filename", protoFileName).WithField("enum_name", enum.GetName()).WithField("jsonschema_filename", jsonSchemaFileName).Info("Generating JSON-schema for stand-alone ENUM")

			// Convert the ENUM:
			enumJSONSchema, err := c.convertEnumType(enum, ConverterFlags{})
			if err != nil {
				switch err {
				case errIgnored:
					continue // This ENUM was marked as ignore - move on to the next
				default:
					c.logger.WithError(err).WithField("proto_filename", protoFileName).Error("Failed to convert")
					return nil, err
				}
			}
			enumJSONSchema.Version = c.schemaVersion

			// Marshal the JSON-Schema into JSON:
			jsonSchemaJSON, err := json.MarshalIndent(enumJSONSchema, "", "    ")
			if err != nil {
				c.logger.WithError(err).Error("Failed to encode jsonSchema")
				return nil, err
			}

			// Add a response:
			resFile := &plugin.CodeGeneratorResponse_File{
				Name:    proto.String(jsonSchemaFileName),
				Content: proto.String(string(jsonSchemaJSON)),
			}
			response = append(response, resFile)
		}
	} else {
		// Otherwise process MESSAGES (packages):
		pkg, ok := c.relativelyLookupPackage(globalPkg, file.GetPackage())
		if !ok {
			return nil, fmt.Errorf("no such package found: %s", file.GetPackage())
		}

		// Go through all of the messages in this file:
		for _, msgDesc := range file.GetMessageType() {

			// Check for our custom message options:
			if opts := msgDesc.GetOptions(); opts != nil && proto.HasExtension(opts, protoc_gen_jsonschema.E_MessageOptions) {
				if opt := proto.GetExtension(opts, protoc_gen_jsonschema.E_MessageOptions); opt != nil {
					if messageOptions, ok := opt.(*protoc_gen_jsonschema.MessageOptions); ok {

						// "Ignored" messages are simply skipped:
						if messageOptions.GetIgnore() {
							c.logger.WithField("msg_name", msgDesc.GetName()).Debug("Skipping ignored message")
							continue
						}
					}
				}
			}

			// skip if we are only generating schema for specific messages
			if genSpecificMessages && !contains(c.messageTargets, msgDesc.GetName()) {
				continue
			}

			// Convert the message:
			messageJSONSchema, err := c.convertMessageType(pkg, msgDesc)
			if err != nil {
				c.logger.WithError(err).WithField("proto_filename", protoFileName).Error("Failed to convert")
				return nil, err
			}

			// Generate a schema filename:
			jsonSchemaFileName := c.generateSchemaFilename(file, fileExtension, msgDesc.GetName())
			c.logger.WithField("proto_filename", protoFileName).WithField("msg_name", msgDesc.GetName()).WithField("jsonschema_filename", jsonSchemaFileName).Info("Generating JSON-schema for MESSAGE")

			// Marshal the JSON-Schema into JSON:
			jsonSchemaJSON, err := json.MarshalIndent(messageJSONSchema, "", "    ")
			if err != nil {
				c.logger.WithError(err).Error("Failed to encode jsonSchema")
				return nil, err
			}

			// Add a response:
			resFile := &plugin.CodeGeneratorResponse_File{
				Name:    proto.String(jsonSchemaFileName),
				Content: proto.String(string(jsonSchemaJSON)),
			}
			response = append(response, resFile)
		}
	}

	return response, nil
}

// convert processes a protoc CodeGeneratorRequest:
func (c *Converter) convert(request *plugin.CodeGeneratorRequest) (*plugin.CodeGeneratorResponse, error) {
	response := &plugin.CodeGeneratorResponse{}

	// Parse the various generator parameter flags:
	c.parseGeneratorParameters(request.GetParameter())

	// Prepare a list of target files:
	generateTargets := make(map[string]bool)
	for _, file := range request.GetFileToGenerate() {
		generateTargets[file] = true
	}

	// Get the source-code info (we use this to map any code comments to JSONSchema descriptions):
	c.sourceInfo = newSourceCodeInfo(request.GetProtoFile())

	// Go through the list of proto files provided by protoc:
	for _, fileDesc := range request.GetProtoFile() {

		// Start with the default / global file extension:
		fileExtension := c.schemaFileExtension

		// Check for our custom file options:
		if opts := fileDesc.GetOptions(); opts != nil && proto.HasExtension(opts, protoc_gen_jsonschema.E_FileOptions) {
			if opt := proto.GetExtension(opts, protoc_gen_jsonschema.E_FileOptions); opt != nil {
				if fileOptions, ok := opt.(*protoc_gen_jsonschema.FileOptions); ok {

					// "Ignored" files are simply skipped:
					if fileOptions.GetIgnore() {
						c.logger.WithField("file_name", fileDesc.GetName()).Debug("Skipping ignored file")
						continue
					}

					// Allow the file extension option to take precedence:
					if fileOptions.GetExtension() != "" {
						fileExtension = fileOptions.GetExtension()
						c.logger.WithField("file_name", fileDesc.GetName()).WithField("extension", fileExtension).Debug("Using optional extension")
					}
				}
			}
		}

		// Check that this file has a proto package, and give it one if not:
		if fileDesc.GetPackage() == "" {
			c.logger.WithField("filename", fileDesc.GetName()).WithField("default_package_name", defaultPackageName).Debug("Proto file doesn't specify a package - assuming the default")
			fileDesc.Package = strPtr(defaultPackageName)
		}

		// Build a list of any messages specified by this file:
		for _, msgDesc := range fileDesc.GetMessageType() {
			c.logger.WithField("msg_name", msgDesc.GetName()).WithField("package_name", fileDesc.GetPackage()).Debug("Loading a message")
			c.registerType(fileDesc.GetPackage(), msgDesc)
		}

		// Build a list of any enums specified by this file:
		for _, en := range fileDesc.GetEnumType() {
			c.logger.WithField("enum_name", en.GetName()).WithField("package_name", fileDesc.GetPackage()).Debug("Loading an enum")
			c.registerEnum(fileDesc.GetPackage(), en)
		}

		// Generate schemas for this file:
		if _, ok := generateTargets[fileDesc.GetName()]; ok {
			c.logger.WithField("filename", fileDesc.GetName()).Debug("Converting file")
			converted, err := c.convertFile(fileDesc, fileExtension)
			if err != nil {
				response.Error = proto.String(fmt.Sprintf("Failed to convert %s: %v", fileDesc.GetName(), err))
				return response, err
			}
			response.File = append(response.File, converted...)
		}
	}

	// This is required in order to "support" optional proto3 fields:
	// https://chromium.googlesource.com/external/github.com/protocolbuffers/protobuf/+/refs/heads/master/docs/implementing_proto3_presence.md
	response.SupportedFeatures = &gengo.SupportedFeatures

	return response, nil
}

func (c *Converter) generateSchemaFilename(file *descriptor.FileDescriptorProto, fileExtension, protoName string) string {
	if c.Flags.PrefixSchemaFilesWithPackage {
		return fmt.Sprintf("%s/%s.%s", file.GetPackage(), protoName, fileExtension)
	}
	return fmt.Sprintf("%s.%s", protoName, fileExtension)
}

func contains(haystack []string, needle string) bool {
	for i := 0; i < len(haystack); i++ {
		if haystack[i] == needle {
			return true
		}
	}

	return false
}
