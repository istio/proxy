package testdata

const OneOf = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OneOf",
    "definitions": {
        "OneOf": {
            "properties": {
                "bar": {
                    "$ref": "#/definitions/samples.OneOf.Bar",
                    "additionalProperties": true
                },
                "baz": {
                    "$ref": "#/definitions/samples.OneOf.Baz",
                    "additionalProperties": true
                },
                "something": {
                    "type": "boolean"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "oneOf": [
                {
                    "required": [
                        "bar"
                    ]
                },
                {
                    "required": [
                        "baz"
                    ]
                }
            ],
            "title": "One Of"
        },
        "samples.OneOf.Bar": {
            "required": [
                "foo"
            ],
            "properties": {
                "foo": {
                    "type": "integer"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Bar"
        },
        "samples.OneOf.Baz": {
            "required": [
                "foo"
            ],
            "properties": {
                "foo": {
                    "type": "string"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Baz"
        }
    }
}`

const OneOfFail = `{
	"something": true,
	"bar": {"foo": 1},
	"baz": {"foo": "one"}
}`

const OneOfPass = `{
	"something": true,
	"bar": {"foo": 1}
}`
