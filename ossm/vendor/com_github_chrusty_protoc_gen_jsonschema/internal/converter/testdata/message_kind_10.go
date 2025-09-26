package testdata

const MessageKind10 = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/MessageKind10",
    "definitions": {
        "MessageKind10": {
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
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Message Kind 10"
        }
    }
}`
