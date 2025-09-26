package testdata

const EnumWithMessage = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/WithFooBarBaz",
    "definitions": {
        "WithFooBarBaz": {
            "properties": {
                "enumField": {
                    "enum": [
                        "Foo",
                        0,
                        "Bar",
                        1,
                        "Baz",
                        2
                    ],
                    "oneOf": [
                        {
                            "type": "string"
                        },
                        {
                            "type": "integer"
                        }
                    ],
                    "title": "Foo Bar Baz"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "With Foo Bar Baz"
        }
    }
}`

const EnumWithMessageFail = `{"enumField": 4}`

const EnumWithMessagePass = `{"enumField": 2}`
