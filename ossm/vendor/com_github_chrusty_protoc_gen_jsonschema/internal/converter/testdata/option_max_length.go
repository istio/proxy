package testdata

const OptionMaxLength = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OptionMaxLength",
    "definitions": {
        "OptionMaxLength": {
            "required": [
                "query"
            ],
            "properties": {
                "query": {
                    "maxLength": 10,
                    "type": "string"
                },
                "result_per_page": {
                    "type": "integer"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Option Max Length"
        }
    }
}`

const OptionMaxLengthFail = `{
    "query": "abcdefghijklmnopqrstuvwxyz",
	"page_number": 4
}`

const OptionMaxLengthPass = `{
	"query": "abc",
	"page_number": 4
}`
