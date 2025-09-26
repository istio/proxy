package testdata

const OptionRequiredMessage = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OptionRequiredMessage",
    "definitions": {
        "OptionRequiredMessage": {
            "required": [
                "name2",
                "timestamp2",
                "id2",
                "rating2",
                "complete2"
            ],
            "properties": {
                "name2": {
                    "type": "string"
                },
                "timestamp2": {
                    "type": "string"
                },
                "id2": {
                    "type": "integer"
                },
                "rating2": {
                    "type": "number"
                },
                "complete2": {
                    "type": "boolean"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Option Required Message"
        }
    }
}`

const OptionRequiredMessageFail = `{
	"name2": "some name",
	"id2": 1
}`

const OptionRequiredMessagePass = `{
	"name2": "some name",
	"timestamp2": "1970-01-01T00:00:00Z",
	"id2": 1,
	"rating2": 100,
	"complete2": true
}`
