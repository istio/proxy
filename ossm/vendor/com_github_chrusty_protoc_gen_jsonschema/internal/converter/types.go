package converter

import (
	"encoding/json"
	"fmt"
	"strings"

	"github.com/alecthomas/jsonschema"
	"github.com/iancoleman/orderedmap"
	"github.com/xeipuuv/gojsonschema"
	"google.golang.org/protobuf/proto"
	descriptor "google.golang.org/protobuf/types/descriptorpb"

	protoc_gen_jsonschema "github.com/chrusty/protoc-gen-jsonschema"
	protoc_gen_validate "github.com/envoyproxy/protoc-gen-validate/validate"
)

var (
	globalPkg = newProtoPackage(nil, "")

	wellKnownTypes = map[string]bool{
		"BoolValue":   true,
		"BytesValue":  true,
		"DoubleValue": true,
		"Duration":    true,
		"FloatValue":  true,
		"Int32Value":  true,
		"Int64Value":  true,
		"ListValue":   true,
		"StringValue": true,
		"Struct":      true,
		"UInt32Value": true,
		"UInt64Value": true,
		"Value":       true,
	}
)

func (c *Converter) registerEnum(pkgName string, enum *descriptor.EnumDescriptorProto) {
	pkg := globalPkg
	if pkgName != "" {
		for _, node := range strings.Split(pkgName, ".") {
			if pkg == globalPkg && node == "" {
				// Skips leading "."
				continue
			}
			child, ok := pkg.children[node]
			if !ok {
				child = newProtoPackage(pkg, node)
				pkg.children[node] = child
			}
			pkg = child
		}
	}
	pkg.enums[enum.GetName()] = enum
}

func (c *Converter) registerType(pkgName string, msgDesc *descriptor.DescriptorProto) {
	pkg := globalPkg
	if pkgName != "" {
		for _, node := range strings.Split(pkgName, ".") {
			if pkg == globalPkg && node == "" {
				// Skips leading "."
				continue
			}
			child, ok := pkg.children[node]
			if !ok {
				child = newProtoPackage(pkg, node)
				pkg.children[node] = child
			}
			pkg = child
		}
	}
	pkg.types[msgDesc.GetName()] = msgDesc
}

// Convert a proto "field" (essentially a type-switch with some recursion):
func (c *Converter) convertField(curPkg *ProtoPackage, desc *descriptor.FieldDescriptorProto, msgDesc *descriptor.DescriptorProto, duplicatedMessages map[*descriptor.DescriptorProto]string, messageFlags ConverterFlags) (*jsonschema.Type, error) {

	// Prepare a new jsonschema.Type for our eventual return value:
	jsonSchemaType := &jsonschema.Type{}

	// Generate a description from src comments (if available)
	if src := c.sourceInfo.GetField(desc); src != nil {
		jsonSchemaType.Title, jsonSchemaType.Description = c.formatTitleAndDescription(nil, src)
	}

	// Switch the types, and pick a JSONSchema equivalent:
	switch desc.GetType() {

	// Float32:
	case descriptor.FieldDescriptorProto_TYPE_DOUBLE,
		descriptor.FieldDescriptorProto_TYPE_FLOAT:
		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				{Type: gojsonschema.TYPE_NUMBER},
			}
		} else {
			jsonSchemaType.Type = gojsonschema.TYPE_NUMBER
		}

	// Int32:
	case descriptor.FieldDescriptorProto_TYPE_INT32,
		descriptor.FieldDescriptorProto_TYPE_UINT32,
		descriptor.FieldDescriptorProto_TYPE_FIXED32,
		descriptor.FieldDescriptorProto_TYPE_SFIXED32,
		descriptor.FieldDescriptorProto_TYPE_SINT32:
		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				{Type: gojsonschema.TYPE_INTEGER},
			}
		} else {
			jsonSchemaType.Type = gojsonschema.TYPE_INTEGER
		}

	// Int64:
	case descriptor.FieldDescriptorProto_TYPE_INT64,
		descriptor.FieldDescriptorProto_TYPE_UINT64,
		descriptor.FieldDescriptorProto_TYPE_FIXED64,
		descriptor.FieldDescriptorProto_TYPE_SFIXED64,
		descriptor.FieldDescriptorProto_TYPE_SINT64:

		// As integer:
		if c.Flags.DisallowBigIntsAsStrings {
			if messageFlags.AllowNullValues {
				jsonSchemaType.OneOf = []*jsonschema.Type{
					{Type: gojsonschema.TYPE_INTEGER},
					{Type: gojsonschema.TYPE_NULL},
				}
			} else {
				jsonSchemaType.Type = gojsonschema.TYPE_INTEGER
			}
		}

		// As string:
		if !c.Flags.DisallowBigIntsAsStrings {
			if messageFlags.AllowNullValues {
				jsonSchemaType.OneOf = []*jsonschema.Type{
					{Type: gojsonschema.TYPE_STRING},
					{Type: gojsonschema.TYPE_NULL},
				}
			} else {
				jsonSchemaType.Type = gojsonschema.TYPE_STRING
			}
		}

	// String:
	case descriptor.FieldDescriptorProto_TYPE_STRING:
		stringDef := &jsonschema.Type{Type: gojsonschema.TYPE_STRING}

		// Custom field options from protoc-gen-jsonschema:
		if opt := proto.GetExtension(desc.GetOptions(), protoc_gen_jsonschema.E_FieldOptions); opt != nil {
			if fieldOptions, ok := opt.(*protoc_gen_jsonschema.FieldOptions); ok {
				stringDef.MinLength = int(fieldOptions.GetMinLength())
				stringDef.MaxLength = int(fieldOptions.GetMaxLength())
				stringDef.Pattern = fieldOptions.GetPattern()
			}
		}

		// Custom field options from protoc-gen-validate:
		if opt := proto.GetExtension(desc.GetOptions(), protoc_gen_validate.E_Rules); opt != nil {
			if fieldRules, ok := opt.(*protoc_gen_validate.FieldRules); fieldRules != nil && ok {
				if stringRules := fieldRules.GetString_(); stringRules != nil {
					stringDef.MaxLength = int(stringRules.GetMaxLen())
					stringDef.MinLength = int(stringRules.GetMinLen())
					stringDef.Pattern = stringRules.GetPattern()
				}
			}
		}

		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				stringDef,
			}
		} else {
			jsonSchemaType.Type = stringDef.Type
			jsonSchemaType.MinLength = stringDef.MinLength
			jsonSchemaType.MaxLength = stringDef.MaxLength
			jsonSchemaType.Pattern = stringDef.Pattern
		}

	// Bytes:
	case descriptor.FieldDescriptorProto_TYPE_BYTES:
		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				{
					Type:           gojsonschema.TYPE_STRING,
					Format:         "binary",
					BinaryEncoding: "base64",
				},
			}
		} else {
			jsonSchemaType.Type = gojsonschema.TYPE_STRING
			jsonSchemaType.Format = "binary"
			jsonSchemaType.BinaryEncoding = "base64"
		}

	// ENUM:
	case descriptor.FieldDescriptorProto_TYPE_ENUM:

		// Go through all the enums we have, see if we can match any to this field.
		fullEnumIdentifier := strings.TrimPrefix(desc.GetTypeName(), ".")
		matchedEnum, _, ok := c.lookupEnum(curPkg, fullEnumIdentifier)
		if !ok {
			return nil, fmt.Errorf("unable to resolve enum type: %s", desc.GetType().String())
		}

		// We already have a converter for standalone ENUMs, so just use that:
		enumSchema, err := c.convertEnumType(matchedEnum, messageFlags)
		if err != nil {
			switch err {
			case errIgnored:
			default:
				return nil, err
			}
		}

		jsonSchemaType = &enumSchema

	// Bool:
	case descriptor.FieldDescriptorProto_TYPE_BOOL:
		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				{Type: gojsonschema.TYPE_BOOLEAN},
			}
		} else {
			jsonSchemaType.Type = gojsonschema.TYPE_BOOLEAN
		}

	// Group (object):
	case descriptor.FieldDescriptorProto_TYPE_GROUP, descriptor.FieldDescriptorProto_TYPE_MESSAGE:

		switch desc.GetTypeName() {
		// Make sure that durations match a particular string pattern (eg 3.4s):
		case ".google.protobuf.Duration":
			jsonSchemaType.Type = gojsonschema.TYPE_STRING
			jsonSchemaType.Format = "regex"
			jsonSchemaType.Pattern = `^([0-9]+\.?[0-9]*|\.[0-9]+)s$`
		case ".google.protobuf.Timestamp":
			jsonSchemaType.Type = gojsonschema.TYPE_STRING
			jsonSchemaType.Format = "date-time"
		case ".google.protobuf.Value", ".google.protobuf.Struct":
			jsonSchemaType.Type = gojsonschema.TYPE_OBJECT
			jsonSchemaType.AdditionalProperties = []byte("true")
		default:
			jsonSchemaType.Type = gojsonschema.TYPE_OBJECT
			if desc.GetLabel() == descriptor.FieldDescriptorProto_LABEL_OPTIONAL {
				jsonSchemaType.AdditionalProperties = []byte("true")
			}
			if desc.GetLabel() == descriptor.FieldDescriptorProto_LABEL_REQUIRED {
				jsonSchemaType.AdditionalProperties = []byte("false")
			}
			if messageFlags.DisallowAdditionalProperties {
				jsonSchemaType.AdditionalProperties = []byte("false")
			}
		}

	default:
		return nil, fmt.Errorf("unrecognized field type: %s", desc.GetType().String())
	}

	// Recurse basic array:
	if desc.GetLabel() == descriptor.FieldDescriptorProto_LABEL_REPEATED && jsonSchemaType.Type != gojsonschema.TYPE_OBJECT {
		jsonSchemaType.Items = &jsonschema.Type{}

		// Custom field options from protoc-gen-validate:
		if opt := proto.GetExtension(desc.GetOptions(), protoc_gen_validate.E_Rules); opt != nil {
			if fieldRules, ok := opt.(*protoc_gen_validate.FieldRules); fieldRules != nil && ok {
				if repeatedRules := fieldRules.GetRepeated(); repeatedRules != nil {
					jsonSchemaType.MaxItems = int(repeatedRules.GetMaxItems())
					jsonSchemaType.MinItems = int(repeatedRules.GetMinItems())
				}
			}
		}

		if len(jsonSchemaType.Enum) > 0 {
			jsonSchemaType.Items.Enum = jsonSchemaType.Enum
			jsonSchemaType.Enum = nil
			jsonSchemaType.Items.OneOf = nil
		} else {
			jsonSchemaType.Items.Type = jsonSchemaType.Type
			jsonSchemaType.Items.OneOf = jsonSchemaType.OneOf
		}

		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				{Type: gojsonschema.TYPE_ARRAY},
			}
		} else {
			jsonSchemaType.Type = gojsonschema.TYPE_ARRAY
			jsonSchemaType.OneOf = []*jsonschema.Type{}
		}
		return jsonSchemaType, nil
	}

	// Recurse nested objects / arrays of objects (if necessary):
	if jsonSchemaType.Type == gojsonschema.TYPE_OBJECT {

		recordType, pkgName, ok := c.lookupType(curPkg, desc.GetTypeName())
		if !ok {
			return nil, fmt.Errorf("no such message type named %s", desc.GetTypeName())
		}

		// Recurse the recordType:
		recursedJSONSchemaType, err := c.recursiveConvertMessageType(curPkg, recordType, pkgName, duplicatedMessages, false)
		if err != nil {
			return nil, err
		}

		// Maps, arrays, and objects are structured in different ways:
		switch {

		// Maps:
		case recordType.Options.GetMapEntry():
			c.logger.
				WithField("field_name", recordType.GetName()).
				WithField("msgDesc_name", *msgDesc.Name).
				Tracef("Is a map")

			if recursedJSONSchemaType.Properties == nil {
				return nil, fmt.Errorf("Unable to find properties of MAP type")
			}

			// Make sure we have a "value":
			value, valuePresent := recursedJSONSchemaType.Properties.Get("value")
			if !valuePresent {
				return nil, fmt.Errorf("Unable to find 'value' property of MAP type")
			}

			// Marshal the "value" properties to JSON (because that's how we can pass on AdditionalProperties):
			additionalPropertiesJSON, err := json.Marshal(value)
			if err != nil {
				return nil, err
			}
			jsonSchemaType.AdditionalProperties = additionalPropertiesJSON

		// Arrays:
		case desc.GetLabel() == descriptor.FieldDescriptorProto_LABEL_REPEATED:
			jsonSchemaType.Items = recursedJSONSchemaType
			jsonSchemaType.Type = gojsonschema.TYPE_ARRAY

			// Build up the list of required fields:
			if messageFlags.AllFieldsRequired && len(recursedJSONSchemaType.OneOf) == 0 && recursedJSONSchemaType.Properties != nil {
				for _, property := range recursedJSONSchemaType.Properties.Keys() {
					jsonSchemaType.Items.Required = append(jsonSchemaType.Items.Required, property)
				}
			}
			jsonSchemaType.Items.Required = dedupe(jsonSchemaType.Items.Required)

		// Not maps, not arrays:
		default:

			// If we've got optional types then just take those:
			if recursedJSONSchemaType.OneOf != nil {
				return recursedJSONSchemaType, nil
			}

			// If we're not an object then set the type from whatever we recursed:
			if recursedJSONSchemaType.Type != gojsonschema.TYPE_OBJECT {
				jsonSchemaType.Type = recursedJSONSchemaType.Type
			}

			// Assume the attrbutes of the recursed value:
			jsonSchemaType.Properties = recursedJSONSchemaType.Properties
			jsonSchemaType.Ref = recursedJSONSchemaType.Ref
			jsonSchemaType.Required = recursedJSONSchemaType.Required

			// Build up the list of required fields:
			if messageFlags.AllFieldsRequired && len(recursedJSONSchemaType.OneOf) == 0 && recursedJSONSchemaType.Properties != nil {
				for _, property := range recursedJSONSchemaType.Properties.Keys() {
					jsonSchemaType.Required = append(jsonSchemaType.Required, property)
				}
			}
		}

		// Optionally allow NULL values:
		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_NULL},
				{Type: jsonSchemaType.Type},
			}
			jsonSchemaType.Type = ""
		}
	}

	jsonSchemaType.Required = dedupe(jsonSchemaType.Required)

	return jsonSchemaType, nil
}

// Converts a proto "MESSAGE" into a JSON-Schema:
func (c *Converter) convertMessageType(curPkg *ProtoPackage, msgDesc *descriptor.DescriptorProto) (*jsonschema.Schema, error) {

	// Get a list of any nested messages in our schema:
	duplicatedMessages, err := c.findNestedMessages(curPkg, msgDesc)
	if err != nil {
		return nil, err
	}

	// Build up a list of JSONSchema type definitions for every message:
	definitions := jsonschema.Definitions{}
	for refmsgDesc, name := range duplicatedMessages {
		refType, err := c.recursiveConvertMessageType(curPkg, refmsgDesc, "", duplicatedMessages, true)
		if err != nil {
			return nil, err
		}

		// Add the schema to our definitions:
		definitions[name] = refType
	}

	// Put together a JSON schema with our discovered definitions, and a $ref for the root type:
	newJSONSchema := &jsonschema.Schema{
		Type: &jsonschema.Type{
			Ref:     fmt.Sprintf("%s%s", c.refPrefix, msgDesc.GetName()),
			Version: c.schemaVersion,
		},
		Definitions: definitions,
	}

	return newJSONSchema, nil
}

// findNestedMessages takes a message, and returns a map mapping pointers to messages nested within it:
// these messages become definitions which can be referenced (instead of repeating them every time they're used)
func (c *Converter) findNestedMessages(curPkg *ProtoPackage, msgDesc *descriptor.DescriptorProto) (map[*descriptor.DescriptorProto]string, error) {

	// Get a list of all nested messages, and how often they occur:
	nestedMessages := make(map[*descriptor.DescriptorProto]string)
	if err := c.recursiveFindNestedMessages(curPkg, msgDesc, msgDesc.GetName(), nestedMessages); err != nil {
		return nil, err
	}

	// Now filter them:
	result := make(map[*descriptor.DescriptorProto]string)
	for message, messageName := range nestedMessages {
		if !message.GetOptions().GetMapEntry() && !strings.HasPrefix(messageName, ".google.protobuf.") {
			result[message] = strings.TrimLeft(messageName, ".")
		}
	}

	return result, nil
}

func (c *Converter) recursiveFindNestedMessages(curPkg *ProtoPackage, msgDesc *descriptor.DescriptorProto, typeName string, nestedMessages map[*descriptor.DescriptorProto]string) error {
	if _, present := nestedMessages[msgDesc]; present {
		return nil
	}
	nestedMessages[msgDesc] = typeName

	for _, desc := range msgDesc.GetField() {
		descType := desc.GetType()
		if descType != descriptor.FieldDescriptorProto_TYPE_MESSAGE && descType != descriptor.FieldDescriptorProto_TYPE_GROUP {
			// no nested messages
			continue
		}

		typeName := desc.GetTypeName()
		recordType, _, ok := c.lookupType(curPkg, typeName)
		if !ok {
			return fmt.Errorf("no such message type named %s", typeName)
		}
		if err := c.recursiveFindNestedMessages(curPkg, recordType, typeName, nestedMessages); err != nil {
			return err
		}
	}

	return nil
}

func (c *Converter) recursiveConvertMessageType(curPkg *ProtoPackage, msgDesc *descriptor.DescriptorProto, pkgName string, duplicatedMessages map[*descriptor.DescriptorProto]string, ignoreDuplicatedMessages bool) (*jsonschema.Type, error) {

	// Prepare a new jsonschema:
	jsonSchemaType := new(jsonschema.Type)

	// Set some per-message flags from config and options:
	messageFlags := c.Flags

	// Custom message options from protoc-gen-jsonschema:
	if opt := proto.GetExtension(msgDesc.GetOptions(), protoc_gen_jsonschema.E_MessageOptions); opt != nil {
		if messageOptions, ok := opt.(*protoc_gen_jsonschema.MessageOptions); ok {

			// AllFieldsRequired:
			if messageOptions.GetAllFieldsRequired() {
				messageFlags.AllFieldsRequired = true
			}

			// AllowNullValues:
			if messageOptions.GetAllowNullValues() {
				messageFlags.AllowNullValues = true
			}

			// DisallowAdditionalProperties:
			if messageOptions.GetDisallowAdditionalProperties() {
				messageFlags.DisallowAdditionalProperties = true
			}

			// ENUMs as constants:
			if messageOptions.GetEnumsAsConstants() {
				messageFlags.EnumsAsConstants = true
			}
		}
	}

	// Generate a description from src comments (if available)
	if src := c.sourceInfo.GetMessage(msgDesc); src != nil {
		jsonSchemaType.Title, jsonSchemaType.Description = c.formatTitleAndDescription(strPtr(msgDesc.GetName()), src)
	}

	// Handle google's well-known types:
	if msgDesc.Name != nil && wellKnownTypes[*msgDesc.Name] && pkgName == ".google.protobuf" {
		switch *msgDesc.Name {
		case "DoubleValue", "FloatValue":
			jsonSchemaType.Type = gojsonschema.TYPE_NUMBER
		case "Int32Value", "UInt32Value":
			jsonSchemaType.Type = gojsonschema.TYPE_INTEGER
		case "Int64Value", "UInt64Value":
			// BigInt as ints
			if messageFlags.DisallowBigIntsAsStrings {
				jsonSchemaType.Type = gojsonschema.TYPE_INTEGER
			} else {

				// BigInt as strings
				jsonSchemaType.Type = gojsonschema.TYPE_STRING
			}

		case "BoolValue":
			jsonSchemaType.Type = gojsonschema.TYPE_BOOLEAN
		case "BytesValue", "StringValue":
			jsonSchemaType.Type = gojsonschema.TYPE_STRING
		case "Value":
			jsonSchemaType.OneOf = []*jsonschema.Type{
				{Type: gojsonschema.TYPE_ARRAY},
				{Type: gojsonschema.TYPE_BOOLEAN},
				{Type: gojsonschema.TYPE_NUMBER},
				{Type: gojsonschema.TYPE_OBJECT},
				{Type: gojsonschema.TYPE_STRING},
			}
			// jsonSchemaType.AdditionalProperties = []byte("true")
		case "Duration":
			jsonSchemaType.Type = gojsonschema.TYPE_STRING
		case "Struct":
			jsonSchemaType.Type = gojsonschema.TYPE_OBJECT
			// jsonSchemaType.AdditionalProperties = []byte("true")
		case "ListValue":
			jsonSchemaType.Type = gojsonschema.TYPE_ARRAY
		}

		// If we're allowing nulls then prepare a OneOf:
		if messageFlags.AllowNullValues {
			jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Type: gojsonschema.TYPE_NULL}, &jsonschema.Type{Type: jsonSchemaType.Type})
			// and clear the Type that was previously set.
			jsonSchemaType.Type = ""
			return jsonSchemaType, nil
		}

		// Otherwise just return this simple type:
		return jsonSchemaType, nil
	}

	// Set defaults:
	jsonSchemaType.Properties = orderedmap.New()

	// Look up references:
	if refName, ok := duplicatedMessages[msgDesc]; ok && !ignoreDuplicatedMessages {
		return &jsonschema.Type{
			Ref: fmt.Sprintf("%s%s", c.refPrefix, refName),
		}, nil
	}

	// Optionally allow NULL values:
	if messageFlags.AllowNullValues {
		jsonSchemaType.OneOf = []*jsonschema.Type{
			{Type: gojsonschema.TYPE_NULL},
			{Type: gojsonschema.TYPE_OBJECT},
		}
	} else {
		jsonSchemaType.Type = gojsonschema.TYPE_OBJECT
	}

	// disallowAdditionalProperties will prevent validation where extra fields are found (outside of the schema):
	if messageFlags.DisallowAdditionalProperties {
		jsonSchemaType.AdditionalProperties = []byte("false")
	} else {
		jsonSchemaType.AdditionalProperties = []byte("true")
	}

	c.logger.WithField("message_str", msgDesc.String()).Trace("Converting message")
	for _, fieldDesc := range msgDesc.GetField() {

		// Custom field options from protoc-gen-jsonschema:
		if opt := proto.GetExtension(fieldDesc.GetOptions(), protoc_gen_jsonschema.E_FieldOptions); opt != nil {
			if fieldOptions, ok := opt.(*protoc_gen_jsonschema.FieldOptions); ok {

				// "Ignored" fields are simply skipped:
				if fieldOptions.GetIgnore() {
					c.logger.WithField("field_name", fieldDesc.GetName()).WithField("message_name", msgDesc.GetName()).Debug("Skipping ignored field")
					continue
				}

				// "Required" fields are added to the list of required attributes in our schema:
				if fieldOptions.GetRequired() {
					c.logger.WithField("field_name", fieldDesc.GetName()).WithField("message_name", msgDesc.GetName()).Debug("Marking required field")
					if c.Flags.UseJSONFieldnamesOnly {
						jsonSchemaType.Required = append(jsonSchemaType.Required, fieldDesc.GetJsonName())
					} else {
						jsonSchemaType.Required = append(jsonSchemaType.Required, fieldDesc.GetName())
					}
				}
			}
		}

		// Convert the field into a JSONSchema type:
		recursedJSONSchemaType, err := c.convertField(curPkg, fieldDesc, msgDesc, duplicatedMessages, messageFlags)
		if err != nil {
			c.logger.WithError(err).WithField("field_name", fieldDesc.GetName()).WithField("message_name", msgDesc.GetName()).Error("Failed to convert field")
			return nil, err
		}
		c.logger.WithField("field_name", fieldDesc.GetName()).WithField("type", recursedJSONSchemaType.Type).Trace("Converted field")

		// If this field is part of a OneOf declaration then build that here:
		if c.Flags.EnforceOneOf && fieldDesc.OneofIndex != nil {
			jsonSchemaType.OneOf = append(jsonSchemaType.OneOf, &jsonschema.Type{Required: []string{fieldDesc.GetName()}})
		}

		// Figure out which field names we want to use:
		switch {
		case c.Flags.UseJSONFieldnamesOnly:
			jsonSchemaType.Properties.Set(fieldDesc.GetJsonName(), recursedJSONSchemaType)
		case c.Flags.UseProtoAndJSONFieldNames:
			jsonSchemaType.Properties.Set(fieldDesc.GetName(), recursedJSONSchemaType)
			jsonSchemaType.Properties.Set(fieldDesc.GetJsonName(), recursedJSONSchemaType)
		default:
			jsonSchemaType.Properties.Set(fieldDesc.GetName(), recursedJSONSchemaType)
		}

		// Enforce all_fields_required:
		if messageFlags.AllFieldsRequired && len(jsonSchemaType.OneOf) == 0 && jsonSchemaType.Properties != nil {
			for _, property := range jsonSchemaType.Properties.Keys() {
				jsonSchemaType.Required = append(jsonSchemaType.Required, property)
			}
		}

		// Look for required fields by the proto2 "required" flag:
		if fieldDesc.GetLabel() == descriptor.FieldDescriptorProto_LABEL_REQUIRED && fieldDesc.OneofIndex == nil {
			if c.Flags.UseJSONFieldnamesOnly {
				jsonSchemaType.Required = append(jsonSchemaType.Required, fieldDesc.GetJsonName())
			} else {
				jsonSchemaType.Required = append(jsonSchemaType.Required, fieldDesc.GetName())
			}
		}
	}

	// Remove empty properties to keep the final output as clean as possible:
	if len(jsonSchemaType.Properties.Keys()) == 0 {
		jsonSchemaType.Properties = nil
	}

	// Dedupe required fields:
	jsonSchemaType.Required = dedupe(jsonSchemaType.Required)

	return jsonSchemaType, nil
}

func dedupe(inputStrings []string) []string {
	appended := make(map[string]bool)
	outputStrings := []string{}

	for _, inputString := range inputStrings {
		if !appended[inputString] {
			outputStrings = append(outputStrings, inputString)
			appended[inputString] = true
		}
	}
	return outputStrings
}
