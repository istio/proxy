package testdata

const FileOptions = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/FileOptions",
    "definitions": {
        "FileOptions": {
            "properties": {
                "ignore": {
                    "type": "boolean",
                    "description": "Files tagged with this will not be processed"
                },
                "extension": {
                    "type": "string",
                    "description": "Override the default file extension for schemas generated from this file"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "description": "Custom FileOptions"
        }
    }
}`

const FileOptionsFail = `{"ignore": 12345}`

const FileOptionsPass = `{"ignore": true}`
