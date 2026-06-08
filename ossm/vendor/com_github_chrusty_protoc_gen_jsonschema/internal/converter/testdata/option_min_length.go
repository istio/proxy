package testdata

const OptionMinLength = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OptionMinLength",
    "definitions": {
        "OptionMinLength": {
            "required": [
                "query"
            ],
            "properties": {
                "query": {
                    "minLength": 2,
                    "type": "string"
                },
                "result_per_page": {
                    "type": "integer"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Option Min Length"
        }
    }
}`

const OptionMinLengthFail = `{
    "query": "a",
	"page_number": 4
}`

const OptionMinLengthPass = `{
	"query": "what?",
	"page_number": 4
}`
