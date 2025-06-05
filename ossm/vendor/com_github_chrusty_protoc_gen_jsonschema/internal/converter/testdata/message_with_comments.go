package testdata

const MessageWithComments = `{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "$ref": "#/definitions/MessageWithComments",
    "definitions": {
        "MessageWithComments": {
            "properties": {
                "name1": {
                    "type": "string",
                    "description": "This field is supposed to represent blahblahblah"
                },
                "excludedComment": {
                    "type": "string"
                }
            },
            "additionalProperties": true,
            "type": "object",
            "title": "This is a leading detached comment (which becomes the title)",
            "description": "This is a leading detached comment (which becomes the title)  This is a message level comment and talks about what this message is and why you should care about it!"
        }
    }
}`

const MessageWithCommentsFail = `{"name1": 12345}`
