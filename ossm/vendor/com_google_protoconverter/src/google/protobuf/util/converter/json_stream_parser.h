/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_STREAM_PARSER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_STREAM_PARSER_H_

#include <cstdint>
#include <stack>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class ObjectWriter;

// A JSON parser that can parse a stream of JSON chunks rather than needing the
// entire JSON string up front. It is a modified version of the parser in
// //net/proto/json/json-parser.h that has been changed in the following ways:
// - Changed from recursion to an explicit stack to allow resumption
// - Added support for int64 and uint64 numbers
// - Removed support for octal and decimal escapes
// - Removed support for numeric keys
// - Removed support for functions (javascript)
// - Removed some lax-comma support (but kept trailing comma support)
// - Writes directly to an ObjectWriter rather than using subclassing
//
// Here is an example usage:
// JsonStreamParser parser(ow_.get());
// util::Status result = parser.Parse(chunk1);
// result.Update(parser.Parse(chunk2));
// result.Update(parser.FinishParse());
// ABSL_DCHECK(result.ok()) << "Failed to parse JSON";
//
// This parser is thread-compatible as long as only one thread is calling a
// Parse() method at a time.
class JsonStreamParser {
 public:
  // Creates a JsonStreamParser that will write to the given ObjectWriter.
  explicit JsonStreamParser(ObjectWriter* ow);
  JsonStreamParser() = delete;
  JsonStreamParser(const JsonStreamParser&) = delete;
  JsonStreamParser& operator=(const JsonStreamParser&) = delete;
  virtual ~JsonStreamParser();

  // Parses a UTF-8 encoded JSON string from an absl::string_view. If the
  // returned status is non-ok, the status might contain a payload
  // ParseErrorType with type_url kParseErrorTypeUrl and a payload containing
  // string snippet of the error with type_url kParseErrorSnippetUrl.
  absl::Status Parse(absl::string_view json);

  // Finish parsing the JSON string. If the returned status is non-ok, the
  // status might contain a payload ParseErrorType with type_url
  // kParseErrorTypeUrl and a payload containing string snippet of the error
  // with type_url kParseErrorSnippetUrl.
  absl::Status FinishParse();

  // Sets the max recursion depth of JSON message to be deserialized. JSON
  // messages over this depth will fail to be deserialized.
  // Default value is 100.
  void set_max_recursion_depth(int max_depth) {
    max_recursion_depth_ = max_depth;
  }

  // Denotes the cause of error.
  enum ParseErrorType {
    UNKNOWN_PARSE_ERROR,
    OCTAL_OR_HEX_ARE_NOT_VALID_JSON_VALUES,
    EXPECTED_COLON,
    EXPECTED_COMMA_OR_BRACKET,
    EXPECTED_VALUE,
    EXPECTED_COMMA_OR_BRACES,
    EXPECTED_OBJECT_KEY_OR_BRACES,
    EXPECTED_VALUE_OR_BRACKET,
    INVALID_KEY_OR_VARIABLE_NAME,
    NON_UTF_8,
    PARSING_TERMINATED_BEFORE_END_OF_INPUT,
    UNEXPECTED_TOKEN,
    EXPECTED_CLOSING_QUOTE,
    ILLEGAL_HEX_STRING,
    INVALID_ESCAPE_SEQUENCE,
    MISSING_LOW_SURROGATE,
    INVALID_LOW_SURROGATE,
    INVALID_UNICODE,
    UNABLE_TO_PARSE_NUMBER,
    NUMBER_EXCEEDS_RANGE_DOUBLE
  };

 private:
  friend class JsonStreamParserTest;
  // Return the current recursion depth.
  int recursion_depth() { return recursion_depth_; }

  enum TokenType {
    BEGIN_STRING,     // " or '
    BEGIN_NUMBER,     // - or digit
    BEGIN_TRUE,       // true
    BEGIN_FALSE,      // false
    BEGIN_NULL,       // null
    BEGIN_OBJECT,     // {
    END_OBJECT,       // }
    BEGIN_ARRAY,      // [
    END_ARRAY,        // ]
    ENTRY_SEPARATOR,  // :
    VALUE_SEPARATOR,  // ,
    BEGIN_KEY,        // letter, _, $ or digit.  Must begin with non-digit
    UNKNOWN           // Unknown token or we ran out of the stream.
  };

  enum ParseType {
    VALUE,        // Expects a {, [, true, false, null, string or number
    OBJ_MID,      // Expects a ',' or }
    ENTRY,        // Expects a key or }
    ENTRY_MID,    // Expects a :
    ARRAY_VALUE,  // Expects a value or ]
    ARRAY_MID     // Expects a ',' or ]
  };

  // Holds the result of parsing a number
  struct NumberResult {
    enum Type { DOUBLE, INT, UINT };
    Type type;
    union {
      double double_val;
      int64_t int_val;
      uint64_t uint_val;
    };
  };

  // Parses a single chunk of JSON, returning an error if the JSON was invalid.
  absl::Status ParseChunk(absl::string_view chunk);

  // Runs the parser based on stack_ and p_, until the stack is empty or p_ runs
  // out of data. If we unexpectedly run out of p_ we push the latest back onto
  // the stack and return.
  absl::Status RunParser();

  // Parses a value from p_ and writes it to ow_.
  // A value may be an object, array, true, false, null, string or number.
  absl::Status ParseValue(TokenType type);

  // Parses a string and writes it out to the ow_.
  absl::Status ParseString();

  // Parses a string, storing the result in parsed_.
  absl::Status ParseStringHelper();

  // This function parses unicode escape sequences in strings. It returns an
  // error when there's a parsing error, either the size is not the expected
  // size or a character is not a hex digit.  When it returns str will contain
  // what has been successfully parsed so far.
  absl::Status ParseUnicodeEscape();

  // Expects p_ to point to a JSON number, writes the number to the writer using
  // the appropriate Render method based on the type of number.
  absl::Status ParseNumber();

  // Parse a number into a NumberResult, reporting an error if no number could
  // be parsed. This method will try to parse into a uint64, int64, or double
  // based on whether the number was positive or negative or had a decimal
  // component.
  absl::Status ParseNumberHelper(NumberResult* result);

  // Parse a number as double into a NumberResult.
  absl::Status ParseDoubleHelper(const std::string& number,
                                 NumberResult* result);

  // Handles a { during parsing of a value.
  absl::Status HandleBeginObject();

  // Parses from the ENTRY state.
  absl::Status ParseEntry(TokenType type);

  // Parses from the ENTRY_MID state.
  absl::Status ParseEntryMid(TokenType type);

  // Parses from the OBJ_MID state.
  absl::Status ParseObjectMid(TokenType type);

  // Handles a [ during parsing of a value.
  absl::Status HandleBeginArray();

  // Parses from the ARRAY_VALUE state.
  absl::Status ParseArrayValue(TokenType type);

  // Parses from the ARRAY_MID state.
  absl::Status ParseArrayMid(TokenType type);

  // Expects p_ to point to an unquoted literal
  absl::Status ParseTrue();
  absl::Status ParseFalse();
  absl::Status ParseNull();
  absl::Status ParseEmptyNull();

  // Whether an empty-null is allowed in the current state.
  bool IsEmptyNullAllowed(TokenType type);

  // Whether the whole input is all whitespaces.
  bool IsInputAllWhiteSpaces(TokenType type);

  // Report a failure as a util::Status.
  absl::Status ReportFailure(absl::string_view message,
                             ParseErrorType parse_code);

  // Report a failure due to an UNKNOWN token type. We check if we hit the
  // end of the stream and if we're finishing or not to detect what type of
  // status to return in this case.
  absl::Status ReportUnknown(absl::string_view message,
                             ParseErrorType parse_code);

  // Helper function to check recursion depth and increment it. It will return
  // OkStatus() if the current depth is allowed. Otherwise an error is returned.
  // key is used for error reporting.
  absl::Status IncrementRecursionDepth(
      absl::optional<absl::string_view> key) const;

  // Advance p_ past all whitespace or until the end of the string.
  void SkipWhitespace();

  // Advance p_ one UTF-8 character
  void Advance();

  // Expects p_ to point to the beginning of a key.
  absl::Status ParseKey();

  // Return the type of the next token at p_.
  TokenType GetNextTokenType();

  // The object writer to write parse events to.
  ObjectWriter* ow_;

  // The stack of parsing we still need to do. When the stack runs empty we will
  // have parsed a single value from the root (e.g. an object or list).
  std::stack<ParseType> stack_;

  // Contains any leftover text from a previous chunk that we weren't able to
  // fully parse, for example the start of a key or number.
  std::string leftover_;

  // The current chunk of JSON being parsed. Primarily used for providing
  // context during error reporting.
  absl::string_view json_;

  // A pointer within the current JSON being parsed, used to track location.
  absl::string_view p_;

  // Stores the last key read, as we separate parsing of keys and values.
  std::string key_;

  // True during the FinishParse() call, so we know that any errors are fatal.
  // For example an unterminated string will normally result in cancelling and
  // trying during the next chunk, but during FinishParse() it is an error.
  bool finishing_;

  // Whether non whitespace tokens have been seen during parsing.
  // It is used to handle the case of a pure whitespace stream input.
  bool seen_non_whitespace_;

  // The JsonStreamParser requires a root element by default and it will raise
  // error if the root element is missing. If `allow_no_root_element_` is true,
  // the JsonStreamParser can also handle this case.
  bool allow_no_root_element_;

  // String we parsed during a call to ParseStringHelper().
  absl::string_view parsed_;

  // Storage for the string we parsed. This may be empty if the string was able
  // to be parsed directly from the input.
  std::string parsed_storage_;

  // The character that opened the string, either ' or ".
  // A value of 0 indicates that string parsing is not in process.
  char string_open_;

  // Storage for the chunk that are being parsed in ParseChunk().
  std::string chunk_storage_;

  // Whether to allow non UTF-8 encoded input and replace invalid code points.
  bool coerce_to_utf8_;

  // Replacement character for invalid UTF-8 code points.
  std::string utf8_replacement_character_;

  // Whether allows empty string represented null array value or object entry
  // value.
  bool allow_empty_null_;

  // Whether unquoted object keys can contain embedded non-alphanumeric
  // characters when this is unambiguous for parsing.
  bool allow_permissive_key_naming_;

  // Whether allows out-of-range floating point numbers or reject them.
  bool loose_float_number_conversion_;

  // Tracks current recursion depth.
  mutable int recursion_depth_;

  // Maximum allowed recursion depth.
  int max_recursion_depth_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_STREAM_PARSER_H_
