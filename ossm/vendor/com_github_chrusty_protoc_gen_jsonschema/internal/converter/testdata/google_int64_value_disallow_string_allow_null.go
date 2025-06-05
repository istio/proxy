package testdata

const GoogleInt64ValueDisallowStringAllowNull = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/GoogleInt64ValueDisallowStringAllowNull",
    "definitions": {
        "GoogleInt64ValueDisallowStringAllowNull": {
            "properties": {
                "big_number": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "integer"
                        }
                    ],
                    "title": "Int 64 Value",
                    "description": "Wrapper message for ` + "`int64`" + `. The JSON representation for ` + "`Int64Value`" + ` is JSON string."
                }
            },
            "additionalProperties": true,
            "oneOf": [
                {
                    "type": "null"
                },
                {
                    "type": "object"
                }
            ],
            "title": "Google Int 64 Value Disallow String Allow Null"
        }
    }
}`

const GoogleInt64ValueDisallowStringAllowNullFail = `{"big_number": "12345"}`

const GoogleInt64ValueDisallowStringAllowNullPass = `{"big_number": null}`
