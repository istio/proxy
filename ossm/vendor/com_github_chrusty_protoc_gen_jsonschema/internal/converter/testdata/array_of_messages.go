package testdata

const ArrayOfMessages = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/ArrayOfMessages",
    "definitions": {
        "ArrayOfMessages": {
            "properties": {
                "description": {
                    "type": "string"
                },
                "payload": {
                    "items": {
                        "$ref": "#/definitions/samples.PayloadMessage"
                    },
                    "type": "array"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Array Of Messages"
        },
        "samples.PayloadMessage": {
            "properties": {
                "name": {
                    "type": "string"
                },
                "timestamp": {
                    "type": "string"
                },
                "id": {
                    "type": "integer"
                },
                "rating": {
                    "type": "number"
                },
                "complete": {
                    "type": "boolean"
                },
                "topology": {
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
            "title": "Payload Message"
        }
    }
}`

const ArrayOfMessagesFail = `{
    "description": "something",
    "payload": [
        {"topology": "cruft"}
    ]
}`

const ArrayOfMessagesPass = `{
    "description": "something",
    "payload": [
        {"topology": "ARRAY_OF_MESSAGE"}
    ]
}`
