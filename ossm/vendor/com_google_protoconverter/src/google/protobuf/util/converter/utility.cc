// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/utility.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/stubs/strutil.h"
#include "google/protobuf/util/converter/constants.h"
#include "google/protobuf/wrappers.pb.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

bool GetBoolOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, bool default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetBoolFromAny(opt->value());
}

int64_t GetInt64OptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, int64_t default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetInt64FromAny(opt->value());
}

double GetDoubleOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, double default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetDoubleFromAny(opt->value());
}

std::string GetStringOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name, absl::string_view default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return std::string(default_value);
  }
  return GetStringFromAny(opt->value());
}

template <typename T>
void ParseFromAny(const std::string& data, T* result) {
  result->ParseFromString(data);
}

// Returns a boolean value contained in Any type.
// TODO(skarvaje): Add type checking & error messages here.
bool GetBoolFromAny(const google::protobuf::Any& any) {
  google::protobuf::BoolValue b;
  ParseFromAny(any.value(), &b);
  return b.value();
}

int64_t GetInt64FromAny(const google::protobuf::Any& any) {
  google::protobuf::Int64Value i;
  ParseFromAny(any.value(), &i);
  return i.value();
}

double GetDoubleFromAny(const google::protobuf::Any& any) {
  google::protobuf::DoubleValue i;
  ParseFromAny(any.value(), &i);
  return i.value();
}

std::string GetStringFromAny(const google::protobuf::Any& any) {
  google::protobuf::StringValue s;
  ParseFromAny(any.value(), &s);
  return s.value();
}

absl::string_view GetTypeWithoutUrl(absl::string_view type_url) {
  if (type_url.size() > kTypeUrlSize && type_url[kTypeUrlSize] == '/') {
    return type_url.substr(kTypeUrlSize + 1);
  } else {
    size_t idx = type_url.rfind('/');
    if (idx != type_url.npos) {
      type_url.remove_prefix(idx + 1);
    }
    return type_url;
  }
}

std::string GetFullTypeWithUrl(absl::string_view simple_type) {
  return absl::StrCat(kTypeServiceBaseUrl, "/", simple_type);
}

const google::protobuf::Option* FindOptionOrNull(
    const RepeatedPtrField<google::protobuf::Option>& options,
    absl::string_view option_name) {
  for (int i = 0; i < options.size(); ++i) {
    const google::protobuf::Option& opt = options.Get(i);
    if (opt.name() == option_name) {
      return &opt;
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindFieldInTypeOrNull(
    const google::protobuf::Type* type, absl::string_view field_name) {
  if (type != nullptr) {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      if (field.name() == field_name) {
        return &field;
      }
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindJsonFieldInTypeOrNull(
    const google::protobuf::Type* type, absl::string_view json_name) {
  if (type != nullptr) {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      if (field.json_name() == json_name) {
        return &field;
      }
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindFieldInTypeByNumberOrNull(
    const google::protobuf::Type* type, int32_t number) {
  if (type != nullptr) {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      if (field.number() == number) {
        return &field;
      }
    }
  }
  return nullptr;
}

const google::protobuf::EnumValue* FindEnumValueByNameOrNull(
    const google::protobuf::Enum* enum_type, absl::string_view enum_name) {
  if (enum_type != nullptr) {
    for (int i = 0; i < enum_type->enumvalue_size(); ++i) {
      const google::protobuf::EnumValue& enum_value = enum_type->enumvalue(i);
      if (enum_value.name() == enum_name) {
        return &enum_value;
      }
    }
  }
  return nullptr;
}

const google::protobuf::EnumValue* FindEnumValueByNumberOrNull(
    const google::protobuf::Enum* enum_type, int32_t value) {
  if (enum_type != nullptr) {
    for (int i = 0; i < enum_type->enumvalue_size(); ++i) {
      const google::protobuf::EnumValue& enum_value = enum_type->enumvalue(i);
      if (enum_value.number() == value) {
        return &enum_value;
      }
    }
  }
  return nullptr;
}

const google::protobuf::EnumValue* FindEnumValueByNameWithoutUnderscoreOrNull(
    const google::protobuf::Enum* enum_type, absl::string_view enum_name) {
  if (enum_type != nullptr) {
    for (int i = 0; i < enum_type->enumvalue_size(); ++i) {
      const google::protobuf::EnumValue& enum_value = enum_type->enumvalue(i);
      std::string enum_name_without_underscore = enum_value.name();

      // Remove underscore from the name.
      enum_name_without_underscore.erase(
          std::remove(enum_name_without_underscore.begin(),
                      enum_name_without_underscore.end(), '_'),
          enum_name_without_underscore.end());
      // Make the name uppercase.
      for (std::string::iterator it = enum_name_without_underscore.begin();
           it != enum_name_without_underscore.end(); ++it) {
        *it = absl::ascii_toupper(*it);
      }

      if (enum_name_without_underscore == enum_name) {
        return &enum_value;
      }
    }
  }
  return nullptr;
}

std::string EnumValueNameToLowerCamelCase(absl::string_view input) {
  return ToCamelCase(absl::AsciiStrToLower(input));
}

std::string ToCamelCase(absl::string_view input) {
  bool capitalize_next = false;
  bool was_cap = true;
  bool is_cap = false;
  bool first_word = true;
  std::string result;
  result.reserve(input.size());

  for (size_t i = 0; i < input.size(); ++i, was_cap = is_cap) {
    is_cap = absl::ascii_isupper(input[i]);
    if (input[i] == '_') {
      capitalize_next = true;
      if (!result.empty()) first_word = false;
      continue;
    } else if (first_word) {
      // Consider when the current character B is capitalized,
      // first word ends when:
      // 1) following a lowercase:   "...aB..."
      // 2) followed by a lowercase: "...ABc..."
      if (!result.empty() && is_cap &&
          (!was_cap ||
           (i + 1 < input.size() && absl::ascii_islower(input[i + 1])))) {
        first_word = false;
        result.push_back(input[i]);
      } else {
        result.push_back(absl::ascii_tolower(input[i]));
        continue;
      }
    } else if (capitalize_next) {
      capitalize_next = false;
      if (absl::ascii_islower(input[i])) {
        result.push_back(absl::ascii_toupper(input[i]));
        continue;
      } else {
        result.push_back(input[i]);
        continue;
      }
    } else {
      result.push_back(absl::ascii_tolower(input[i]));
    }
  }
  return result;
}

std::string ToSnakeCase(absl::string_view input) {
  bool was_not_underscore = false;  // Initialize to false for case 1 (below)
  bool was_not_cap = false;
  std::string result;
  result.reserve(input.size() << 1);

  for (size_t i = 0; i < input.size(); ++i) {
    if (absl::ascii_isupper(input[i])) {
      // Consider when the current character B is capitalized:
      // 1) At beginning of input:   "B..." => "b..."
      //    (e.g. "Biscuit" => "biscuit")
      // 2) Following a lowercase:   "...aB..." => "...a_b..."
      //    (e.g. "gBike" => "g_bike")
      // 3) At the end of input:     "...AB" => "...ab"
      //    (e.g. "GoogleLAB" => "google_lab")
      // 4) Followed by a lowercase: "...ABc..." => "...a_bc..."
      //    (e.g. "GBike" => "g_bike")
      if (was_not_underscore &&                     //            case 1 out
          (was_not_cap ||                           // case 2 in, case 3 out
           (i + 1 < input.size() &&                 //            case 3 out
            absl::ascii_islower(input[i + 1])))) {  // case 4 in
        // We add an underscore for case 2 and case 4.
        result.push_back('_');
      }
      result.push_back(absl::ascii_tolower(input[i]));
      was_not_underscore = true;
      was_not_cap = false;
    } else {
      result.push_back(input[i]);
      was_not_underscore = input[i] != '_';
      was_not_cap = true;
    }
  }
  return result;
}

absl::flat_hash_set<absl::string_view>* well_known_types_ = nullptr;
absl::once_flag well_known_types_init_;
const char* well_known_types_name_array_[] = {
    "google.protobuf.Timestamp",   "google.protobuf.Duration",
    "google.protobuf.DoubleValue", "google.protobuf.FloatValue",
    "google.protobuf.Int64Value",  "google.protobuf.UInt64Value",
    "google.protobuf.Int32Value",  "google.protobuf.UInt32Value",
    "google.protobuf.BoolValue",   "google.protobuf.StringValue",
    "google.protobuf.BytesValue",  "google.protobuf.FieldMask"};

void DeleteWellKnownTypes() { delete well_known_types_; }

void InitWellKnownTypes() {
  well_known_types_ = new absl::flat_hash_set<absl::string_view>(
      std::begin(well_known_types_name_array_),
      std::end(well_known_types_name_array_));
  google::protobuf::internal::OnShutdown(&DeleteWellKnownTypes);
}

bool IsWellKnownType(const std::string& type_name) {
  absl::call_once(well_known_types_init_, InitWellKnownTypes);
  return well_known_types_->contains(type_name);
}

bool IsValidBoolString(absl::string_view bool_string) {
  return bool_string == "true" || bool_string == "false" ||
         bool_string == "1" || bool_string == "0";
}

bool IsMap(const google::protobuf::Field& field,
           const google::protobuf::Type& type) {
  return field.cardinality() == google::protobuf::Field::CARDINALITY_REPEATED &&
         (GetBoolOptionOrDefault(type.options(), "map_entry", false) ||
          GetBoolOptionOrDefault(type.options(),
                                 "google.protobuf.MessageOptions.map_entry",
                                 false));
}

bool IsMessageSetWireFormat(const google::protobuf::Type& type) {
  return GetBoolOptionOrDefault(type.options(), "message_set_wire_format",
                                false) ||
         GetBoolOptionOrDefault(
             type.options(),
             "google.protobuf.MessageOptions.message_set_wire_format", false);
}

std::string DoubleAsString(double value) {
  if (value == std::numeric_limits<double>::infinity()) return "Infinity";
  if (value == -std::numeric_limits<double>::infinity()) return "-Infinity";
  if (std::isnan(value)) return "NaN";

  return SimpleDtoa(value);
}

std::string FloatAsString(float value) {
  if (std::isfinite(value)) return SimpleFtoa(value);
  return DoubleAsString(value);
}

bool SafeStrToFloat(absl::string_view str, float* value) {
  double double_value;
  if (!safe_strtod(str, &double_value)) {
    return false;
  }

  if (std::isinf(double_value) || std::isnan(double_value)) return false;

  // Fail if the value is not representable in float.
  if (double_value > std::numeric_limits<float>::max() ||
      double_value < -std::numeric_limits<float>::max()) {
    return false;
  }

  *value = static_cast<float>(double_value);
  return true;
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
