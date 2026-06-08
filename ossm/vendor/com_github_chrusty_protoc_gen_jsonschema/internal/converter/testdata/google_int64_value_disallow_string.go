package testdata

const GoogleInt64ValueDisallowString = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/GoogleInt64ValueDisallowString",
    "definitions": {
        "GoogleInt64ValueDisallowString": {
            "properties": {
                "big_number": {
                    "additionalProperties": true,
                    "type": "integer"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Google Int 64 Value Disallow String"
        }
    }
}`

const GoogleInt64ValueDisallowStringFail = `{"big_number": "12345"}`

const GoogleInt64ValueDisallowStringPass = `{"big_number": 12345}`
