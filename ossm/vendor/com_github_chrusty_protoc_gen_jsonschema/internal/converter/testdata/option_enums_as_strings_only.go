package testdata

const OptionEnumsAsStringsOnly = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "enum": [
        "NOT_SPECIFIED",
        "USD",
        "GBP",
        "EUR"
    ],
    "type": "string",
    "title": "Currency"
}`

const OptionEnumsAsStringsOnlyPass = `"NOT_SPECIFIED"`
const OptionEnumsAsStringsOnlyFail = `2`
