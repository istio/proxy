package testdata

const MessageOptions = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/MessageOptions",
    "definitions": {
        "MessageOptions": {
            "properties": {
                "ignore": {
                    "type": "boolean",
                    "description": "Messages tagged with this will not be processed"
                },
                "all_fields_required": {
                    "type": "boolean",
                    "description": "Messages tagged with this will have all fields marked as \"required\":"
                },
                "allow_null_values": {
                    "type": "boolean",
                    "description": "Messages tagged with this will additionally accept null values for all properties:"
                },
                "disallow_additional_properties": {
                    "type": "boolean",
                    "description": "Messages tagged with this will have all fields marked as not allowing additional properties:"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "description": "Custom MessageOptions"
        }
    }
}`

const MessageOptionsFail = `{"ignore": 12345}`

const MessageOptionsPass = `{"ignore": true}`
