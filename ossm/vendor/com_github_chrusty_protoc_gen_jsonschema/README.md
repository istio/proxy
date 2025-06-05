Protobuf to JSON-Schema compiler
================================

This takes protobuf definitions and converts them into JSONSchemas, which can be used to dynamically validate JSON messages.

Useful for people who define their data using ProtoBuf, but use JSON for the "wire" format.

"Heavily influenced" by [Google's protobuf-to-BigQuery-schema compiler](https://github.com/GoogleCloudPlatform/protoc-gen-bq-schema).


Generated Schemas
-----------------

- One JSONSchema file is generated for each root-level proto message and ENUM. These are intended to be stand alone self-contained schemas which can be used to validate a payload derived from their source proto message
- Nested message schemas become [referenced "definitions"](https://cswr.github.io/JsonSchema/spec/definitions_references/). This means that you know the name of the proto message they came from, and their schema is not duplicated (within the context of one JSONSchema file at least)


Logic
-----

- For each proto file provided
  - Generates schema for each ENUM
    - JSONSchema filename deried from ENUM name
  - Generates schema for each Message
    - Builds a list of every nested message and converts them to JSONSchema
    - Recursively converts attributes and nested messages within the root message
      - Optionally makes all fields required
      - Optionally allows NULL values
      - Optionally allows additional properties
      - Optionally marks all fields required
      - Specially marked fields are labelled required (options.proto)
      - Specially marked fields are omitted (options.proto)
      - Special handling for "OneOf"
      - Special handling for arrays
      - Special handling for maps
    - Injects references to nested messages
    - JSONSchema filename derived from Message name
  - Bundles these into a protoc generator response


Installation
------------

> Note: This tool requires Go 1.11+ to be installed.

Install this plugin using Go:

```sh
go install github.com/chrusty/protoc-gen-jsonschema/cmd/protoc-gen-jsonschema@latest
```


Usage
-----

> Note: This plugin requires the [`protoc`](https://github.com/protocolbuffers/protobuf) CLI to be installed.

**protoc-gen-jsonschema** is designed to run like any other proto generator. The following examples show how to use options flags to enable different generator behaviours (more examples in the Makefile too).

```sh
protoc \ # The protobuf compiler
--jsonschema_out=. \ # jsonschema out directory
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto # proto input directories and folders
```


Configuration Parameters
------------------------

The following configuration parameters are supported. They should be added to the protoc command and can be combined as a comma-delimited string. Some examples are included in the following Examples section.

Options can also be provided in this format (which is easier on the eye):

```
protoc \
  --plugin=${HOME}/go/bin/protoc-gen-jsonschema \
  --jsonschema_opt=enforce_oneof
  --jsonschema_opt=file_extension=schema.json \
  --jsonschema_opt=disallow_additional_properties \
  --jsonschema_out=schemas \
  --proto_path=proto
```


| CONFIG | DESCRIPTION |
|--------|-------------|
|`all_fields_required`| Require all fields in schema |
|`allow_null_values`| Allow null values in schema |
|`debug`| Enable debug logging |
|`disallow_additional_properties`| Disallow additional properties in schema |
|`disallow_bigints_as_strings`| Disallow big integers as strings |
|`enforce_oneof`| Interpret Proto "oneOf" clauses |
|`enums_as_strings_only`| Only include strings in the allowed values for enums |
|`file_extension`| Specify a custom file extension for generated schemas |
|`json_fieldnames`| Use JSON field names only |
|`prefix_schema_files_with_package`| Prefix the output filename with package |
|`proto_and_json_fieldnames`| Use proto and JSON field names |


Custom Proto Options
--------------------

If you don't want to use the configuration parameters (admittedly quite a nasty cli syntax) then some of the generator behaviour can be controlled using custom proto options. These are defined in [options.proto](options.proto), and your protoc command will need to include this file. See the [sample protos](internal/converter/testdata/proto) and generator commands in the [Makefile](Makefile).

### Enum Options

These apply to specifically marked enums, giving you more finely-grained control than with the CLI flags.

- [enums_as_constants](internal/converter/testdata/proto/ImportedEnum.proto): Encode ENUMs (and their annotations) as CONST
- [enums_as_strings_only](internal/converter/testdata/proto/OptionEnumsAsStringsOnly.proto): ENUM values are only strings (not the numeric counterparts)
- [enums_trim_prefix](internal/converter/testdata/proto/OptionEnumsTrimPrefix.proto): ENUM values have enum name prefix removed

### Field Options

These apply to specifically marked fields, giving you more finely-grained control than with the CLI flags.

- [ignore](internal/converter/testdata/proto/OptionIgnoredField.proto): Ignore (omit) a specific field
- [required](internal/converter/testdata/proto/OptionRequiredField.proto): Mark a specific field as being REQUIRED

### File Options

These options apply to an entire proto file.

- [ignore](internal/converter/testdata/proto/OptionIgnoredFile.proto): Ignore (skip) a specific file
- [extension](internal/converter/testdata/proto/OptionFileExtension.proto): Specify a custom file-extension for the generated schema for this file

### Message Options

These options apply to a specific proto message.

- [ignore](internal/converter/testdata/proto/OptionIgnoredMessage.proto): Ignore (skip) a specific message
- [all_fields_required](internal/converter/testdata/proto/OptionRequiredMessage.proto): Mark all fields in a specific message as "required"
- [allow_null_values](internal/converter/testdata/proto/OptionAllowNullValues.proto): Additionally allow null values for all fields in a message
- [disallow_additional_properties](internal/converter/testdata/proto/OptionDisallowAdditionalProperties.proto): Only accept the specific properties, no extras
- [enums_as_constants](internal/converter/testdata/proto/OptionEnumsAsConstants.proto): Encode ENUMs (and their annotations) as CONST


Validation Options
------------------

We are also beginning to support validation options from [protoc-gen-validate](https://github.com/bufbuild/protoc-gen-validate).

At the moment the following are supported (but export more in the future):

- Arrays
    - MaxItems
    - MinItems
- Strings
    - MaxLength
    - MinLength
    - Pattern


Examples
--------

### Require all fields

> Because proto3 doesn't accommodate this.

```sh
protoc \
--jsonschema_out=all_fields_required:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Allow NULL values

> By default, JSONSchemas will reject NULL values unless we explicitly allow them

```sh
protoc \
--jsonschema_out=allow_null_values:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Enable debug logging

```sh
protoc \
--jsonschema_out=debug:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Disallow additional properties

> JSONSchemas won't validate JSON containing extra parameters
    
```sh
protoc \
--jsonschema_out=disallow_additional_properties:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Disallow permissive validation of big-integers as strings

> (eg scientific notation)

```sh
protoc \
--jsonschema_out=disallow_bigints_as_strings:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Prefix generated schema files with their package name (as a directory)

```sh
protoc \
--jsonschema_out=prefix_schema_files_with_package:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Target specific messages within a proto file

```sh
# Generates MessageKind10.jsonschema and MessageKind11.jsonschema
# Use this to generate json schema from proto files with multiple messages
# Separate schema names with '+'
protoc \
--jsonschema_out=messages=[MessageKind10+MessageKind11]:. \
--proto_path=testdata/proto testdata/proto/TwelveMessages.proto
```

### Generate fields with JSON names

```sh
protoc \
--jsonschema_out=json_fieldnames:. \
--proto_path=testdata/proto testdata/proto/ArrayOfPrimitives.proto
```

### Custom schema file extension

The default file extension is `json`. You can override that with the "file_extension" parameter.

```sh
protoc \
--jsonschema_out=file_extension=jsonschema:. \
--proto_path=internal/converter/testdata/proto internal/converter/testdata/proto/ArrayOfPrimitives.proto
```


Sample protos (for testing)
---------------------------

* Proto with a simple (flat) structure: [samples.PayloadMessage](internal/converter/testdata/proto/PayloadMessage.proto)
* Proto containing a nested object (defined internally): [samples.NestedObject](internal/converter/testdata/proto/NestedObject.proto)
* Proto containing a nested message (defined in a different proto file): [samples.NestedMessage](internal/converter/testdata/proto/NestedMessage.proto)
* Proto containing an array of a primitive types (string, int): [samples.ArrayOfPrimitives](internal/converter/testdata/proto/ArrayOfPrimitives.proto)
* Proto containing an array of objects (internally defined): [samples.ArrayOfObjects](internal/converter/testdata/proto/ArrayOfObjects.proto)
* Proto containing an array of messages (defined in a different proto file): [samples.ArrayOfMessage](internal/converter/testdata/proto/ArrayOfMessage.proto)
* Proto containing multi-level enums (flat and nested and arrays): [samples.Enumception](internal/converter/testdata/proto/Enumception.proto)
* Proto containing a stand-alone enum: [samples.ImportedEnum](internal/converter/testdata/proto/ImportedEnum.proto)
* Proto containing 2 stand-alone enums: [samples.FirstEnum, samples.SecondEnum](internal/converter/testdata/proto/SeveralEnums.proto)
* Proto containing 2 messages: [samples.FirstMessage, samples.SecondMessage](internal/converter/testdata/proto/SeveralMessages.proto)
* Proto containing 12 messages: [samples.MessageKind1 - samples.MessageKind12](internal/converter/testdata/proto/TwelveMessages.proto)


Links
-----

* [About JSON Schema](http://json-schema.org/)
* [Popular GoLang JSON-Schema validation library](https://github.com/xeipuuv/gojsonschema)
* [Another GoLang JSON-Schema validation library](https://github.com/lestrrat/go-jsschema)
