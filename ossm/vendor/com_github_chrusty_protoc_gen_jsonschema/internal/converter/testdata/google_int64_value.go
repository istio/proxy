package testdata

const GoogleInt64Value = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/GoogleInt64Value",
    "definitions": {
        "GoogleInt64Value": {
            "properties": {
                "big_number": {
                    "additionalProperties": true,
                    "type": "string"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Google Int 64 Value"
        }
    }
}`

const GoogleInt64ValueFail = `{"big_number": 12345}`

const GoogleInt64ValuePass = `{"big_number": "12345"}`
