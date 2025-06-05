package testdata

const ArrayOfObjects = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/ArrayOfObjects",
    "definitions": {
        "ArrayOfObjects": {
            "properties": {
                "description": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "string"
                        }
                    ]
                },
                "payload": {
                    "items": {
                        "$ref": "#/definitions/samples.ArrayOfObjects.RepeatedPayload"
                    },
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "array"
                        }
                    ]
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
            "title": "Array Of Objects"
        },
        "samples.ArrayOfObjects.RepeatedPayload": {
            "properties": {
                "name": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "string"
                        }
                    ]
                },
                "timestamp": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "string"
                        }
                    ]
                },
                "id": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "integer"
                        }
                    ]
                },
                "rating": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "number"
                        }
                    ]
                },
                "complete": {
                    "oneOf": [
                        {
                            "type": "null"
                        },
                        {
                            "type": "boolean"
                        }
                    ]
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
                        },
                        {
                            "type": "null"
                        }
                    ],
                    "title": "Topology"
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
            "title": "Repeated Payload"
        }
    }
}`

const ArrayOfObjectsFail = `{
    "description": "something",
    "payload": [
        {
            "topology": "cruft"
        }
    ]
}`

const ArrayOfObjectsPass = `{
    "description": "something",
    "payload": [
        {
            "topology": "ARRAY_OF_OBJECT"
        }
    ]
}`
