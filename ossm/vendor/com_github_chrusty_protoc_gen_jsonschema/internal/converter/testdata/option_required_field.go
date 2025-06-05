package testdata

const OptionRequiredField = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OptionRequiredField",
    "definitions": {
        "OptionRequiredField": {
            "required": [
                "query",
                "page_number"
            ],
            "properties": {
                "query": {
                    "type": "string"
                },
                "page_number": {
                    "type": "integer"
                },
                "result_per_page": {
                    "type": "integer"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Option Required Field"
        }
    }
}`

const OptionRequiredFieldFail = `{
	"page_number": 4
}`

const OptionRequiredFieldPass = `{
	"query": "what?",
	"page_number": 4
}`
