package testdata

const Proto2Required = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/Proto2Required",
    "definitions": {
        "Proto2Required": {
            "required": [
                "query"
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
            "title": "Proto 2 Required"
        }
    }
}`

const Proto2RequiredFail = `{
	"page_number": 4
}`

const Proto2RequiredPass = `{
	"query": "what?",
	"page_number": 4
}`
