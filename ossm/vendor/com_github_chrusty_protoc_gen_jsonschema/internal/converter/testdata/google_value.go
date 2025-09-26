package testdata

const GoogleValue = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/GoogleValue",
    "definitions": {
        "GoogleValue": {
            "properties": {
                "arg": {
                    "oneOf": [
                        {
                            "type": "array"
                        },
                        {
                            "type": "boolean"
                        },
                        {
                            "type": "number"
                        },
                        {
                            "type": "object"
                        },
                        {
                            "type": "string"
                        }
                    ],
                    "title": "Value",
                    "description": "` + "`Value`" + ` represents a dynamically typed value which can be either null, a number, a string, a boolean, a recursive struct value, or a list of values. A producer of value is expected to set one of these variants. Absence of any variant indicates an error. The JSON representation for ` + "`Value`" + ` is JSON value."
                },
                "some_list": {
                    "additionalProperties": false,
                    "type": "array"
                },
                "some_struct": {
                    "additionalProperties": true,
                    "type": "object"
                }
            },
            "additionalProperties": false,
            "type": "object",
            "title": "Google Value"
        }
    }
}
`

const GoogleValueFail = `{"arg": null, "some_list": 4}`

const GoogleValuePass = `{"arg": 12345, "some_list": [1,2,3,4]}`
