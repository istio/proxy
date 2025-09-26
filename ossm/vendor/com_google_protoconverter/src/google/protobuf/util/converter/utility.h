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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_UTILITY_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_UTILITY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/type.pb.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// Size of "type.googleapis.com"
static const int64_t kTypeUrlSize = 19;

// Finds the tech option identified by option_name. Parses the boolean value and
// returns it.
// When the option with the given name is not found, default_value is returned.
bool GetBoolOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, bool default_value);

// Returns int64 option value. If the option isn't found, returns the
// default_value.
int64_t GetInt64OptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, int64_t default_value);

// Returns double option value. If the option isn't found, returns the
// default_value.
double GetDoubleOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, double default_value);

// Returns string option value. If the option isn't found, returns the
// default_value.
std::string GetStringOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, absl::string_view default_value);

// Returns a boolean value contained in Any type.
// TODO(skarvaje): Make these utilities dealing with Any types more generic,
// add more error checking and move to a more public/shareable location so
// others can use.
bool GetBoolFromAny(const google::protobuf::Any& any);

// Returns int64 value contained in Any type.
int64_t GetInt64FromAny(const google::protobuf::Any& any);

// Returns double value contained in Any type.
double GetDoubleFromAny(const google::protobuf::Any& any);

// Returns string value contained in Any type.
std::string GetStringFromAny(const google::protobuf::Any& any);

// Returns the type string without the url prefix. e.g.: If the passed type is
// 'type.googleapis.com/tech.type.Bool', the returned value is 'tech.type.Bool'.
absl::string_view GetTypeWithoutUrl(absl::string_view type_url);

// Returns the simple_type with the base type url (kTypeServiceBaseUrl)
// prefixed.
//
// E.g:
// GetFullTypeWithUrl("google.protobuf.Timestamp") returns the string
// "type.googleapis.com/google.protobuf.Timestamp".
std::string GetFullTypeWithUrl(absl::string_view simple_type);

// Finds and returns option identified by name and option_name within the
// provided map. Returns nullptr if none found.
const google::protobuf::Option* FindOptionOrNull(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name);

// Finds and returns the field identified by field_name in the passed tech Type
// object. Returns nullptr if none found.
const google::protobuf::Field* FindFieldInTypeOrNull(
    const google::protobuf::Type* type, absl::string_view field_name);

// Similar to FindFieldInTypeOrNull, but this looks up fields with given
// json_name.
const google::protobuf::Field* FindJsonFieldInTypeOrNull(
    const google::protobuf::Type* type, absl::string_view json_name);

// Similar to FindFieldInTypeOrNull, but this looks up fields by number.
const google::protobuf::Field* FindFieldInTypeByNumberOrNull(
    const google::protobuf::Type* type, int32_t number);

// Finds and returns the EnumValue identified by enum_name in the passed tech
// Enum object. Returns nullptr if none found.
const google::protobuf::EnumValue* FindEnumValueByNameOrNull(
    const google::protobuf::Enum* enum_type, absl::string_view enum_name);

// Finds and returns the EnumValue identified by value in the passed tech
// Enum object. Returns nullptr if none found.
const google::protobuf::EnumValue* FindEnumValueByNumberOrNull(
    const google::protobuf::Enum* enum_type, int32_t value);

// Finds and returns the EnumValue identified by enum_name without underscore in
// the passed tech Enum object. Returns nullptr if none found.
// For Ex. if enum_name is ACTIONANDADVENTURE it can get accepted if
// EnumValue's name is action_and_adventure or ACTION_AND_ADVENTURE.
const google::protobuf::EnumValue* FindEnumValueByNameWithoutUnderscoreOrNull(
    const google::protobuf::Enum* enum_type, absl::string_view enum_name);

// Converts input to camel-case and returns it.
std::string ToCamelCase(const absl::string_view input);

// Converts enum name string to camel-case and returns it.
std::string EnumValueNameToLowerCamelCase(const absl::string_view input);

// Converts input to snake_case and returns it.
std::string ToSnakeCase(absl::string_view input);

// Returns true if type_name represents a well-known type.
bool IsWellKnownType(const std::string& type_name);

// Returns true if 'bool_string' represents a valid boolean value. Only "true",
// "false", "0" and "1" are allowed.
bool IsValidBoolString(absl::string_view bool_string);

// Returns true if "field" is a protobuf map field based on its type.
bool IsMap(const google::protobuf::Field& field,
           const google::protobuf::Type& type);

// Returns true if the given type has special MessageSet wire format.
bool IsMessageSetWireFormat(const google::protobuf::Type& type);

// Infinity/NaN-aware conversion to string.
std::string DoubleAsString(double value);
std::string FloatAsString(float value);

// Convert from int32, int64, uint32, uint64, double or float to string.
template <typename T>
std::string ValueAsString(T value) {
  return absl::StrCat(value);
}

template <>
inline std::string ValueAsString(float value) {
  return FloatAsString(value);
}

template <>
inline std::string ValueAsString(double value) {
  return DoubleAsString(value);
}

// Converts a string to float. Unlike safe_strtof, conversion will fail if the
// value fits into double but not float (e.g., DBL_MAX).
bool SafeStrToFloat(absl::string_view str, float* value);

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_UTILITY_H_
