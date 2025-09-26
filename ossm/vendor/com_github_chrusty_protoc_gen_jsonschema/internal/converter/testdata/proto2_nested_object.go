package testdata

const Proto2NestedObject = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/Proto2NestedObject",
    "definitions": {
        "Proto2NestedObject": {
            "required": [
                "payload",
                "description"
            ],
            "properties": {
                "payload": {
                    "$ref": "#/definitions/samples.Proto2NestedObject.NestedPayload",
                    "additionalProperties": false
                },
                "description": {
                    "type": "string"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Proto 2 Nested Object"
        },
        "samples.Proto2NestedObject.NestedPayload": {
            "required": [
                "name",
                "timestamp",
                "id",
                "rating",
                "complete",
                "topology"
            ],
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
            "title": "Nested Payload"
        }
    }
}`

const Proto2NestedObjectFail = `{
	"payload": {
		"topology": "FLAT"	
	}
}`

const Proto2NestedObjectPass = `{
	"description": "lots of attributes",
	"payload": {
		"name": "something",
		"timestamp": "1970-01-01T00:00:00Z",
		"id": 1,
		"rating": 100,
		"complete": true,
		"topology": "FLAT"
	}
}`
