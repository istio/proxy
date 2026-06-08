package testdata

const EnumNestedReference = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/Msg",
    "definitions": {
        "Msg": {
            "properties": {
                "nestedEnumField": {
                    "enum": [
                        "FLAT",
                        0,
                        "NESTED_OBJECT",
                        1,
                        "NESTED_MESSAGE",
                        2,
                        "ARRAY_OF_TYPE",
                        3,
                        "ARRAY_OF_OBJECT",
                        4,
                        "ARRAY_OF_MESSAGE",
                        5
                    ],
                    "oneOf": [
                        {
                            "type": "string"
                        },
                        {
                            "type": "integer"
                        }
                    ],
                    "title": "Topology"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Msg"
        }
    }
}`

const EnumNestedReferenceFail = `{"nestedEnumField": 8}`

const EnumNestedReferencePass = `{"nestedEnumField": "FLAT"}`
