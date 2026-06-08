package testdata

const OptionIgnoredField = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OptionIgnoredField",
    "definitions": {
        "OptionIgnoredField": {
            "properties": {
                "visible1": {
                    "type": "string"
                },
                "visible2": {
                    "type": "string"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Option Ignored Field"
        }
    }
}`

const OptionIgnoredFieldFail = `{"visible1": 12345}`

const OptionIgnoredFieldPass = `{"visible2": "hello"}`
