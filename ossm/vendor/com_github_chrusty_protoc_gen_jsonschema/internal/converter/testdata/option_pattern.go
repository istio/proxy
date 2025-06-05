package testdata

const OptionPattern = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/OptionPattern",
    "definitions": {
        "OptionPattern": {
            "required": [
                "query"
            ],
            "properties": {
                "query": {
                    "pattern": "^(\\([0-9]{3}\\))?[0-9]{3}-[0-9]{4}$",
                    "type": "string"
                },
                "result_per_page": {
                    "type": "integer"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "Option Pattern"
        }
    }
}`

const OptionPatternFail = `{
    "query": "a",
	"page_number": 4
}`

const OptionPatternPass = `{
	"query": "(888)555-1212",
	"page_number": 4
}`
