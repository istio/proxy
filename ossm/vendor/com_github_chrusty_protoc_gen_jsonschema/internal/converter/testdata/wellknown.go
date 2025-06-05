package testdata

const WellKnown = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/WellKnown",
    "definitions": {
        "WellKnown": {
            "properties": {
                "string_value": {
                    "additionalProperties": true,
                    "type": "string"
                },
                "map_of_integers": {
                    "additionalProperties": {
                        "additionalProperties": true,
                        "type": "integer"
                    },
                    "type": "object"
                },
                "map_of_scalar_integers": {
                    "additionalProperties": {
                        "type": "integer"
                    },
                    "type": "object"
                },
                "list_of_integers": {
                    "items": {
                        "type": "integer",
                        "title": "Int 32 Value",
                        "description": "Wrapper message for ` + "`int32`" + `. The JSON representation for ` + "`Int32Value`" + ` is JSON number."
                    },
                    "type": "array"
                },
                "duration": {
                    "pattern": "^([0-9]+\\.?[0-9]*|\\.[0-9]+)s$",
                    "type": "string",
                    "description": "This is a duration:",
                    "format": "regex"
                },
                "struct": {
                    "additionalProperties": true,
                    "type": "object"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Well Known"
        }
    }
}`

const WellKnownFail = `{"duration": "9"}`

const WellKnownPass = `{"duration": "9s"}`
