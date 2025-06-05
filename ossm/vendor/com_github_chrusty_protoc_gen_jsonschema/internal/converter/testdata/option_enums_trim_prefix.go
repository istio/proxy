package testdata

const OptionEnumsTrimPrefix = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "enum": [
        "UNSPECIFIED",
        "HTTP",
        "HTTPS"
    ],
    "type": "string",
    "title": "Scheme"
}`

const OptionEnumsTrimPrefixPass = `"HTTP"`

const OptionEnumsTrimPrefixFail = `4`
