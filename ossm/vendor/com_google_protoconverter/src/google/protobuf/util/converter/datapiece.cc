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

#include "google/protobuf/util/converter/datapiece.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/stubs/strutil.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/utility.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace {

template <typename T>
int Sign(const T x) {
  if (x == T(0)) return 0;
  return x > T(0) ? 1 : -1;
}

template <typename To, typename From>
absl::StatusOr<To> ValidateNumberConversion(To after, From before) {
  if (after == before && Sign<From>(before) == Sign<To>(after)) {
    return after;
  } else {
    return absl::InvalidArgumentError(
        std::is_integral<From>::value       ? ValueAsString(before)
        : std::is_same<From, double>::value ? DoubleAsString(before)
                                            : FloatAsString(before));
  }
}

// For general conversion between
//     int32, int64, uint32, uint64, double and float
// except conversion between double and float.
template <typename To, typename From>
absl::StatusOr<To> NumberConvertAndCheck(From before) {
  if (std::is_same<From, To>::value) return before;

  To after = static_cast<To>(before);
  return ValidateNumberConversion(after, before);
}

// For conversion to integer types (int32, int64, uint32, uint64) from floating
// point types (double, float) only.
template <typename To, typename From>
absl::StatusOr<To> FloatingPointToIntConvertAndCheck(From before) {
  if (std::is_same<From, To>::value) return before;

  To after = static_cast<To>(before);
  return ValidateNumberConversion(after, before);
}

// For conversion between double and float only.
absl::StatusOr<double> FloatToDouble(float before) {
  // Casting float to double should just work as double has more precision
  // than float.
  return static_cast<double>(before);
}

absl::StatusOr<float> DoubleToFloat(double before) {
  if (std::isnan(before)) {
    return std::numeric_limits<float>::quiet_NaN();
  } else if (!std::isfinite(before)) {
    // Converting a double +inf/-inf to float should just work.
    return static_cast<float>(before);
  } else if (before > std::numeric_limits<float>::max() ||
             before < -std::numeric_limits<float>::max()) {
    // Some doubles are larger than the largest float, but after
    // rounding they will be equal to the largest float.
    // We can't just attempt the conversion because that has UB if
    // the value really is out-of-range.
    // Here we take advantage that 1/2-ing a large floating point
    // will not lose precision.
    double half_before = before * 0.5;
    if (half_before < std::numeric_limits<float>::max() &&
        half_before > -std::numeric_limits<float>::max()) {
      const float half_fmax = std::numeric_limits<float>::max() * 0.5f;
      // If after being cut in half, the value is less than the largest float,
      // then it's safe to convert it to float.  Importantly, this conversion
      // rounds in the same way that the original does.
      float half_after = static_cast<float>(half_before);
      if (half_after <= half_fmax && half_after >= -half_fmax) {
        return half_after + half_after;
      }
    }
    // Double value outside of the range of float.
    return absl::InvalidArgumentError(DoubleAsString(before));
  } else {
    return static_cast<float>(before);
  }
}

}  // namespace

absl::StatusOr<int32_t> DataPiece::ToInt32() const {
  if (type_ == TYPE_STRING)
    return StringToNumber<int32_t>(safe_strto32);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<int32_t, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<int32_t, float>(float_);

  return GenericConvert<int32_t>();
}

absl::StatusOr<uint32_t> DataPiece::ToUint32() const {
  if (type_ == TYPE_STRING)
    return StringToNumber<uint32_t>(safe_strtou32);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<uint32_t, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<uint32_t, float>(float_);

  return GenericConvert<uint32_t>();
}

absl::StatusOr<int64_t> DataPiece::ToInt64() const {
  if (type_ == TYPE_STRING)
    return StringToNumber<int64_t>(safe_strto64);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<int64_t, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<int64_t, float>(float_);

  return GenericConvert<int64_t>();
}

absl::StatusOr<uint64_t> DataPiece::ToUint64() const {
  if (type_ == TYPE_STRING)
    return StringToNumber<uint64_t>(safe_strtou64);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<uint64_t, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<uint64_t, float>(float_);

  return GenericConvert<uint64_t>();
}

absl::StatusOr<double> DataPiece::ToDouble() const {
  if (type_ == TYPE_FLOAT) {
    return FloatToDouble(float_);
  }
  if (type_ == TYPE_STRING) {
    if (str_ == "Infinity") return std::numeric_limits<double>::infinity();
    if (str_ == "-Infinity") return -std::numeric_limits<double>::infinity();
    if (str_ == "NaN") return std::numeric_limits<double>::quiet_NaN();
    absl::StatusOr<double> value = StringToNumber<double>(safe_strtod);
    if (value.ok() && !std::isfinite(value.value())) {
      // safe_strtod converts out-of-range values to +inf/-inf, but we want
      // to report them as errors.
      return absl::InvalidArgumentError(absl::StrCat("\"", str_, "\""));
    } else {
      return value;
    }
  }
  return GenericConvert<double>();
}

absl::StatusOr<float> DataPiece::ToFloat() const {
  if (type_ == TYPE_DOUBLE) {
    return DoubleToFloat(double_);
  }
  if (type_ == TYPE_STRING) {
    if (str_ == "Infinity") return std::numeric_limits<float>::infinity();
    if (str_ == "-Infinity") return -std::numeric_limits<float>::infinity();
    if (str_ == "NaN") return std::numeric_limits<float>::quiet_NaN();
    // SafeStrToFloat() is used instead of safe_strtof() because the later
    // does not fail on inputs like SimpleDtoa(DBL_MAX).
    return StringToNumber<float>(SafeStrToFloat);
  }
  return GenericConvert<float>();
}

absl::StatusOr<bool> DataPiece::ToBool() const {
  switch (type_) {
    case TYPE_BOOL:
      return bool_;
    case TYPE_STRING:
      // Calls out to absl::SimpleAtob, which supports "true"/"false",
      // "yes"/"no", "y"/"n", "t"/"f", and "1"/"0".
      return StringToNumber<bool>(safe_strtob);
    default:
      break;
  }
  return absl::InvalidArgumentError(
      ValueAsStringOrDefault("Wrong type. Cannot convert to Bool."));
}

absl::StatusOr<std::string> DataPiece::ToString() const {
  switch (type_) {
    case TYPE_STRING:
      return std::string(str_);
    case TYPE_BYTES: {
      std::string base64;
      absl::Base64Escape(str_, &base64);
      return base64;
    }
    default:
      return absl::InvalidArgumentError(
          ValueAsStringOrDefault("Cannot convert to string."));
  }
}

std::string DataPiece::ValueAsStringOrDefault(
    absl::string_view default_string) const {
  switch (type_) {
    case TYPE_INT32:
      return absl::StrCat(i32_);
    case TYPE_INT64:
      return absl::StrCat(i64_);
    case TYPE_UINT32:
      return absl::StrCat(u32_);
    case TYPE_UINT64:
      return absl::StrCat(u64_);
    case TYPE_DOUBLE:
      return DoubleAsString(double_);
    case TYPE_FLOAT:
      return FloatAsString(float_);
    case TYPE_BOOL:
      return bool_ ? "true" : "false";
    case TYPE_STRING:
      return absl::StrCat("\"", str_, "\"");
    case TYPE_BYTES: {
      std::string base64;
      absl::WebSafeBase64Escape(str_, &base64);
      return absl::StrCat("\"", base64, "\"");
    }
    case TYPE_NULL:
      return "null";
    default:
      return std::string(default_string);
  }
}

absl::StatusOr<std::string> DataPiece::ToBytes() const {
  if (type_ == TYPE_BYTES) return std::string(str_);
  if (type_ == TYPE_STRING) {
    std::string decoded;
    if (!DecodeBase64(str_, &decoded)) {
      return absl::InvalidArgumentError(
          ValueAsStringOrDefault("Invalid data in input."));
    }
    return decoded;
  } else {
    return absl::InvalidArgumentError(ValueAsStringOrDefault(
        "Wrong type. Only String or Bytes can be converted to Bytes."));
  }
}

absl::StatusOr<int> DataPiece::ToEnum(const google::protobuf::Enum* enum_type,
                                      bool use_lower_camel_for_enums,
                                      bool case_insensitive_enum_parsing,
                                      bool ignore_unknown_enum_values,
                                      bool* is_unknown_enum_value) const {
  if (type_ == TYPE_NULL) return google::protobuf::NULL_VALUE;

  if (type_ == TYPE_STRING) {
    // First try the given value as a name.
    std::string enum_name = std::string(str_);
    const google::protobuf::EnumValue* value =
        FindEnumValueByNameOrNull(enum_type, enum_name);
    if (value != nullptr) return value->number();

    // Check if int version of enum is sent as string.
    absl::StatusOr<int32_t> int_value = ToInt32();
    if (int_value.ok()) {
      if (const google::protobuf::EnumValue* enum_value =
              FindEnumValueByNumberOrNull(enum_type, int_value.value())) {
        return enum_value->number();
      }
    }

    // Next try a normalized name.
    bool should_normalize_enum =
        case_insensitive_enum_parsing || use_lower_camel_for_enums;
    if (should_normalize_enum) {
      for (std::string::iterator it = enum_name.begin(); it != enum_name.end();
           ++it) {
        *it = *it == '-' ? '_' : absl::ascii_toupper(*it);
      }
      value = FindEnumValueByNameOrNull(enum_type, enum_name);
      if (value != nullptr) return value->number();
    }

    // If use_lower_camel_for_enums is true try with enum name without
    // underscore. This will also accept camel case names as the enum_name has
    // been normalized before.
    if (use_lower_camel_for_enums) {
      value = FindEnumValueByNameWithoutUnderscoreOrNull(enum_type, enum_name);
      if (value != nullptr) return value->number();
    }

    // If ignore_unknown_enum_values is true an unknown enum value is ignored.
    if (ignore_unknown_enum_values) {
      *is_unknown_enum_value = true;
      if (enum_type->enumvalue_size() > 0) {
        return enum_type->enumvalue(0).number();
      }
    }
  } else {
    // We don't need to check whether the value is actually declared in the
    // enum because we preserve unknown enum values as well.
    return ToInt32();
  }
  return absl::InvalidArgumentError(
      ValueAsStringOrDefault("Cannot find enum with given value."));
}

template <typename To>
absl::StatusOr<To> DataPiece::GenericConvert() const {
  switch (type_) {
    case TYPE_INT32:
      return NumberConvertAndCheck<To, int32_t>(i32_);
    case TYPE_INT64:
      return NumberConvertAndCheck<To, int64_t>(i64_);
    case TYPE_UINT32:
      return NumberConvertAndCheck<To, uint32_t>(u32_);
    case TYPE_UINT64:
      return NumberConvertAndCheck<To, uint64_t>(u64_);
    case TYPE_DOUBLE:
      return NumberConvertAndCheck<To, double>(double_);
    case TYPE_FLOAT:
      return NumberConvertAndCheck<To, float>(float_);
    default:  // TYPE_ENUM, TYPE_STRING, TYPE_CORD, TYPE_BOOL
      return absl::InvalidArgumentError(ValueAsStringOrDefault(
          "Wrong type. Bool, Enum, String and Cord not supported in "
          "GenericConvert."));
  }
}

template <typename To>
absl::StatusOr<To> DataPiece::StringToNumber(bool (*func)(absl::string_view,
                                                          To*)) const {
  if (str_.size() > 0 && (str_[0] == ' ' || str_[str_.size() - 1] == ' ')) {
    return absl::InvalidArgumentError(absl::StrCat("\"", str_, "\""));
  }
  To result;
  if (func(str_, &result)) return result;
  return absl::InvalidArgumentError(
      absl::StrCat("\"", std::string(str_), "\""));
}

bool DataPiece::DecodeBase64(absl::string_view src, std::string* dest) const {
  // Try web-safe decode first, if it fails, try the non-web-safe decode.
  if (absl::WebSafeBase64Unescape(src, dest)) {
    if (use_strict_base64_decoding_) {
      // In strict mode, check if the escaped version gives us the same value as
      // unescaped.
      std::string encoded;
      // WebSafeBase64Escape does no padding by default.
      absl::WebSafeBase64Escape(*dest, &encoded);
      // Remove trailing padding '=' characters before comparison.
      absl::string_view src_no_padding = absl::string_view(src).substr(
          0, absl::EndsWith(src, "=") ? src.find_last_not_of('=') + 1
                                      : src.length());
      return encoded == src_no_padding;
    }
    return true;
  }

  if (absl::Base64Unescape(src, dest)) {
    if (use_strict_base64_decoding_) {
      std::string encoded;
      Base64Escape(
          reinterpret_cast<const unsigned char*>(dest->data()), dest->length(),
          &encoded, false);
      absl::string_view src_no_padding = absl::string_view(src).substr(
          0, absl::EndsWith(src, "=") ? src.find_last_not_of('=') + 1
                                      : src.length());
      return encoded == src_no_padding;
    }
    return true;
  }

  return false;
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
