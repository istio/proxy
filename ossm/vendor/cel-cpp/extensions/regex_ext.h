// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This extension depends on the CEL optional type. Please ensure that the
// EnableOptionalTypes is called when using regex extensions.
//
// # Replace
//
// The `regex.replace` function replaces all non-overlapping substring of a
// regex pattern in the target string with the given replacement string.
// Optionally, you can limit the number of replacements by providing a count
// argument. When the count is a negative number, the function acts as replace
// all. Only numeric (\N) capture group references are supported in the
// replacement string, with validation for correctness. Backslashed-escaped
// digits (\1 to \9) within the replacement argument can be used to insert text
// matching the corresponding parenthesized group in the regexp pattern. An
// error will be thrown for invalid regex or replace string.
//
//  regex.replace(target: string, pattern: string,
//                replacement: string) -> string
//  regex.replace(target: string, pattern: string,
//                replacement: string, count: int) -> string
//
// Examples:
//
//  regex.replace('hello world hello', 'hello', 'hi') == 'hi world hi'
//  regex.replace('banana', 'a', 'x', 0) == 'banana'
//  regex.replace('banana', 'a', 'x', 1) == 'bxnana'
//  regex.replace('banana', 'a', 'x', -12) == 'bxnxnx'
//  regex.replace('foo bar', '(fo)o (ba)r', r'\2 \1') == 'ba fo'
//  regex.replace('test', '(.)', r'\2') \\ Runtime Error invalid replace
//  string  regex.replace('foo bar', '(', '$2 $1') \\ Runtime Error invalid
//
// # Extract
//
// The `regex.extract` function returns the first match of a regex pattern in a
// string. If no match is found, it returns an optional none value. An error
// will be thrown for invalid regex or for multiple capture groups.
//
//  regex.extract(target: string, pattern: string) -> optional<string>
//
// Examples:
//
//  regex.extract('item-A, item-B', 'item-(\\w+)') == optional.of('A')
//  regex.extract('HELLO', 'hello') == optional.empty()
//  regex.extract('testuser@testdomain', '(.*)@([^.]*)') // Runtime Error
//   multiple capture group
//
// # Extract All
//
// The `regex.extractAll` function returns a list of all matches of a regex
// pattern in a target string. If no matches are found, it returns an empty
// list. An error will be thrown for invalid regex or for multiple capture
// groups.
//
//  regex.extractAll(target: string, pattern: string) -> list<string>
//
// Examples:
//
//  regex.extractAll('id:123, id:456', 'id:\\d+') == ['id:123', 'id:456']
//  regex.extractAll('testuser@testdomain', '(.*)@([^.]*)') // Runtime Error
//   multiple capture group

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_REGEX_EXT_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_REGEX_EXT_H_

#include "absl/status/status.h"
#include "checker/type_checker_builder.h"
#include "compiler/compiler.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "runtime/runtime_builder.h"

namespace cel::extensions {

// Register extension functions for regular expressions for
// google::api::expr::runtime::CelValue runtime.
//
// Note: CelValue does not support optional types, so regex.extract is
// unsupported.
absl::Status RegisterRegexExtensionFunctions(
    google::api::expr::runtime::CelFunctionRegistry* registry,
    const google::api::expr::runtime::InterpreterOptions& options);

// Register extension functions for regular expressions.
absl::Status RegisterRegexExtensionFunctions(RuntimeBuilder& builder);

// Type check declarations for the regex extension library.
// Provides decls for the following functions:
//
// regex.replace(target: str, pattern: str, replacement: str) -> str
//
// regex.replace(target: str, pattern: str, replacement: str, count: int) -> str
//
// regex.extract(target: str, pattern: str) -> optional<str>
//
// regex.extractAll(target: str, pattern: str) -> list<str>
CheckerLibrary RegexExtCheckerLibrary();

// Provides decls for the following functions:
//
// regex.replace(target: str, pattern: str, replacement: str) -> str
//
// regex.replace(target: str, pattern: str, replacement: str, count: int) -> str
//
// regex.extract(target: str, pattern: str) -> optional<str>
//
// regex.extractAll(target: str, pattern: str) -> list<str>
CompilerLibrary RegexExtCompilerLibrary();

}  // namespace cel::extensions
#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_REGEX_EXT_H_
