// Copyright 2024 Google LLC
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

#include "extensions/formatting.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/memory/memory.h"
#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

namespace {

static constexpr int32_t kNanosPerMillisecond = 1000000;
static constexpr int32_t kNanosPerMicrosecond = 1000;

absl::StatusOr<absl::string_view> FormatString(
    const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);

absl::StatusOr<std::pair<int64_t, std::optional<int>>> ParsePrecision(
    absl::string_view format) {
  if (format.empty() || format[0] != '.') return std::pair{0, std::nullopt};

  int64_t i = 1;
  while (i < format.size() && absl::ascii_isdigit(format[i])) {
    ++i;
  }
  if (i == format.size()) {
    return absl::InvalidArgumentError(
        "unable to find end of precision specifier");
  }
  int precision;
  if (!absl::SimpleAtoi(format.substr(1, i - 1), &precision)) {
    return absl::InvalidArgumentError(
        "unable to convert precision specifier to integer");
  }
  return std::pair{i, precision};
}

absl::StatusOr<absl::string_view> FormatDuration(
    const Value& value, std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  absl::Duration duration = value.GetDuration();
  if (duration == absl::ZeroDuration()) {
    return "0s";
  }
  if (duration < absl::ZeroDuration()) {
    scratch.append("-");
    duration = absl::AbsDuration(duration);
  }
  int64_t seconds = absl::ToInt64Seconds(duration);
  absl::StrAppend(&scratch, seconds);
  int64_t nanos = absl::ToInt64Nanoseconds(duration - absl::Seconds(seconds));
  if (nanos != 0) {
    scratch.append(".");
    if (nanos % kNanosPerMillisecond == 0) {
      scratch.append(absl::StrFormat("%03d", nanos / kNanosPerMillisecond));
    } else if (nanos % kNanosPerMicrosecond == 0) {
      scratch.append(absl::StrFormat("%06d", nanos / kNanosPerMicrosecond));
    } else {
      scratch.append(absl::StrFormat("%09d", nanos));
    }
  }
  scratch.append("s");
  return scratch;
}

absl::StatusOr<absl::string_view> FormatDouble(
    double value, std::optional<int> precision, bool use_scientific_notation,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  static constexpr int kDefaultPrecision = 6;
  if (std::isnan(value)) {
    return "NaN";
  } else if (value == std::numeric_limits<double>::infinity()) {
    return "Infinity";
  } else if (value == -std::numeric_limits<double>::infinity()) {
    return "-Infinity";
  }
  auto format = absl::StrCat("%.", precision.value_or(kDefaultPrecision),
                             use_scientific_notation ? "e" : "f");
  if (use_scientific_notation) {
    scratch = absl::StrFormat(*absl::ParsedFormat<'e'>::New(format), value);
  } else {
    scratch = absl::StrFormat(*absl::ParsedFormat<'f'>::New(format), value);
  }
  return scratch;
}

absl::StatusOr<absl::string_view> FormatList(
    const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(auto it, value.GetList().NewIterator());
  scratch.clear();
  scratch.push_back('[');
  std::string value_scratch;

  while (it->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto next,
                         it->Next(descriptor_pool, message_factory, arena));
    absl::string_view next_str;
    value_scratch.clear();
    CEL_ASSIGN_OR_RETURN(
        next_str, FormatString(next, descriptor_pool, message_factory, arena,
                               value_scratch));
    absl::StrAppend(&scratch, next_str);
    absl::StrAppend(&scratch, ", ");
  }
  if (scratch.size() > 1) {
    scratch.resize(scratch.size() - 2);
  }
  scratch.push_back(']');
  return scratch;
}

absl::StatusOr<absl::string_view> FormatMap(
    const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  absl::btree_map<std::string, Value> value_map;
  std::string value_scratch;
  CEL_RETURN_IF_ERROR(value.GetMap().ForEach(
      [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
        if (key.kind() != ValueKind::kString &&
            key.kind() != ValueKind::kBool && key.kind() != ValueKind::kInt &&
            key.kind() != ValueKind::kUint) {
          return absl::InvalidArgumentError(
              absl::StrCat("map keys must be strings, booleans, integers, or "
                           "unsigned integers, was given ",
                           key.GetTypeName()));
        }
        value_scratch.clear();
        CEL_ASSIGN_OR_RETURN(auto key_str,
                             FormatString(key, descriptor_pool, message_factory,
                                          arena, value_scratch));
        value_map.emplace(key_str, value);
        return true;
      },
      descriptor_pool, message_factory, arena));

  scratch.clear();
  scratch.push_back('{');
  for (const auto& [key, value] : value_map) {
    value_scratch.clear();
    CEL_ASSIGN_OR_RETURN(auto value_str,
                         FormatString(value, descriptor_pool, message_factory,
                                      arena, value_scratch));
    absl::StrAppend(&scratch, key, ": ");
    absl::StrAppend(&scratch, value_str);
    absl::StrAppend(&scratch, ", ");
  }
  if (scratch.size() > 1) {
    scratch.resize(scratch.size() - 2);
  }
  scratch.push_back('}');
  return scratch;
}

absl::StatusOr<absl::string_view> FormatString(
    const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  switch (value.kind()) {
    case ValueKind::kList:
      return FormatList(value, descriptor_pool, message_factory, arena,
                        scratch);
    case ValueKind::kMap:
      return FormatMap(value, descriptor_pool, message_factory, arena, scratch);
    case ValueKind::kString:
      return value.GetString().NativeString(scratch);
    case ValueKind::kBytes:
      return value.GetBytes().NativeString(scratch);
    case ValueKind::kNull:
      return "null";
    case ValueKind::kInt:
      absl::StrAppend(&scratch, value.GetInt().NativeValue());
      return scratch;
    case ValueKind::kUint:
      absl::StrAppend(&scratch, value.GetUint().NativeValue());
      return scratch;
    case ValueKind::kDouble: {
      auto number = value.GetDouble().NativeValue();
      if (std::isnan(number)) {
        return "NaN";
      }
      if (number == std::numeric_limits<double>::infinity()) {
        return "Infinity";
      }
      if (number == -std::numeric_limits<double>::infinity()) {
        return "-Infinity";
      }
      absl::StrAppend(&scratch, number);
      return scratch;
    }
    case ValueKind::kTimestamp:
      absl::StrAppend(&scratch, value.DebugString());
      return scratch;
    case ValueKind::kDuration:
      return FormatDuration(value, scratch);
    case ValueKind::kBool:
      if (value.GetBool().NativeValue()) {
        return "true";
      }
      return "false";
    case ValueKind::kType:
      return value.GetType().name();
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "could not convert argument %s to string", value.GetTypeName()));
  }
}

absl::StatusOr<absl::string_view> FormatDecimal(
    const Value& value, std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  scratch.clear();
  switch (value.kind()) {
    case ValueKind::kInt:
      absl::StrAppend(&scratch, value.GetInt().NativeValue());
      return scratch;
    case ValueKind::kUint:
      absl::StrAppend(&scratch, value.GetUint().NativeValue());
      return scratch;
    case ValueKind::kDouble:
      return FormatDouble(value.GetDouble().NativeValue(),
                          /*precision=*/std::nullopt,
                          /*use_scientific_notation=*/false, scratch);
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("decimal clause can only be used on numbers, was given ",
                       value.GetTypeName()));
  }
}

absl::StatusOr<absl::string_view> FormatBinary(
    const Value& value, std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  decltype(value.GetUint().NativeValue()) unsigned_value;
  bool sign_bit = false;
  switch (value.kind()) {
    case ValueKind::kInt: {
      auto tmp = value.GetInt().NativeValue();
      if (tmp < 0) {
        sign_bit = true;
        // Negating min int is undefined behavior, so we need to use unsigned
        // arithmetic.
        using unsigned_type = std::make_unsigned<decltype(tmp)>::type;
        unsigned_value = -static_cast<unsigned_type>(tmp);
      } else {
        unsigned_value = tmp;
      }
      break;
    }
    case ValueKind::kUint:
      unsigned_value = value.GetUint().NativeValue();
      break;
    case ValueKind::kBool:
      if (value.GetBool().NativeValue()) {
        return "1";
      }
      return "0";
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "binary clause can only be used on integers and bools, was given ",
          value.GetTypeName()));
  }

  if (unsigned_value == 0) {
    return "0";
  }

  int size = absl::bit_width(unsigned_value) + sign_bit;
  scratch.resize(size);
  for (int i = size - 1; i >= 0; --i) {
    if (unsigned_value & 1) {
      scratch[i] = '1';
    } else {
      scratch[i] = '0';
    }
    unsigned_value >>= 1;
  }
  if (sign_bit) {
    scratch[0] = '-';
  }
  return scratch;
}

absl::StatusOr<absl::string_view> FormatHex(
    const Value& value, bool use_upper_case,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  switch (value.kind()) {
    case ValueKind::kString:
      scratch = absl::BytesToHexString(value.GetString().NativeString(scratch));
      break;
    case ValueKind::kBytes:
      scratch = absl::BytesToHexString(value.GetBytes().NativeString(scratch));
      break;
    case ValueKind::kInt: {
      // Golang supports signed hex, but absl::StrFormat does not. To be
      // compatible, we need to add a leading '-' if the value is negative.
      auto tmp = value.GetInt().NativeValue();
      if (tmp < 0) {
        // Negating min int is undefined behavior, so we need to use unsigned
        // arithmetic.
        using unsigned_type = std::make_unsigned<decltype(tmp)>::type;
        scratch = absl::StrFormat("-%x", -static_cast<unsigned_type>(tmp));
      } else {
        scratch = absl::StrFormat("%x", tmp);
      }
      break;
    }
    case ValueKind::kUint:
      scratch = absl::StrFormat("%x", value.GetUint().NativeValue());
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("hex clause can only be used on integers, byte buffers, "
                       "and strings, was given ",
                       value.GetTypeName()));
  }
  if (use_upper_case) {
    absl::AsciiStrToUpper(&scratch);
  }
  return scratch;
}

absl::StatusOr<absl::string_view> FormatOctal(
    const Value& value, std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  switch (value.kind()) {
    case ValueKind::kInt: {
      // Golang supports signed octals, but absl::StrFormat does not. To be
      // compatible, we need to add a leading '-' if the value is negative.
      auto tmp = value.GetInt().NativeValue();
      if (tmp < 0) {
        // Negating min int is undefined behavior, so we need to use unsigned
        // arithmetic.
        using unsigned_type = std::make_unsigned<decltype(tmp)>::type;
        scratch = absl::StrFormat("-%o", -static_cast<unsigned_type>(tmp));
      } else {
        scratch = absl::StrFormat("%o", tmp);
      }
      return scratch;
    }
    case ValueKind::kUint:
      scratch = absl::StrFormat("%o", value.GetUint().NativeValue());
      return scratch;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("octal clause can only be used on integers, was given ",
                       value.GetTypeName()));
  }
}

absl::StatusOr<double> GetDouble(const Value& value, std::string& scratch) {
  if (value.kind() == ValueKind::kString) {
    auto str = value.GetString().NativeString(scratch);
    if (str == "NaN") {
      return std::nan("");
    } else if (str == "Infinity") {
      return std::numeric_limits<double>::infinity();
    } else if (str == "-Infinity") {
      return -std::numeric_limits<double>::infinity();
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("only \"NaN\", \"Infinity\", and \"-Infinity\" are "
                       "supported for conversion to double: ",
                       str));
    }
  }
  if (value.kind() != ValueKind::kDouble) {
    return absl::InvalidArgumentError(
        absl::StrCat("expected a double but got a ", value.GetTypeName()));
  }
  return value.GetDouble().NativeValue();
}

absl::StatusOr<absl::string_view> FormatFixed(
    const Value& value, std::optional<int> precision,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(auto number, GetDouble(value, scratch));
  return FormatDouble(number, precision,
                      /*use_scientific_notation=*/false, scratch);
}

absl::StatusOr<absl::string_view> FormatScientific(
    const Value& value, std::optional<int> precision,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(auto number, GetDouble(value, scratch));
  return FormatDouble(number, precision,
                      /*use_scientific_notation=*/true, scratch);
}

absl::StatusOr<std::pair<int64_t, absl::string_view>> ParseAndFormatClause(
    absl::string_view format, const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  CEL_ASSIGN_OR_RETURN(auto precision_pair, ParsePrecision(format));
  auto [read, precision] = precision_pair;
  switch (format[read]) {
    case 's': {
      CEL_ASSIGN_OR_RETURN(auto result,
                           FormatString(value, descriptor_pool, message_factory,
                                        arena, scratch));
      return std::pair{read, result};
    }
    case 'd': {
      CEL_ASSIGN_OR_RETURN(auto result, FormatDecimal(value, scratch));
      return std::pair{read, result};
    }
    case 'f': {
      CEL_ASSIGN_OR_RETURN(auto result, FormatFixed(value, precision, scratch));
      return std::pair{read, result};
    }
    case 'e': {
      CEL_ASSIGN_OR_RETURN(auto result,
                           FormatScientific(value, precision, scratch));
      return std::pair{read, result};
    }
    case 'b': {
      CEL_ASSIGN_OR_RETURN(auto result, FormatBinary(value, scratch));
      return std::pair{read, result};
    }
    case 'x':
    case 'X': {
      CEL_ASSIGN_OR_RETURN(
          auto result,
          FormatHex(value,
                    /*use_upper_case=*/format[read] == 'X', scratch));
      return std::pair{read, result};
    }
    case 'o': {
      CEL_ASSIGN_OR_RETURN(auto result, FormatOctal(value, scratch));
      return std::pair{read, result};
    }
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "unrecognized formatting clause \"%c\"", format[read]));
  }
}

absl::StatusOr<Value> Format(
    const StringValue& format_value, const ListValue& args,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string format_scratch, clause_scratch;
  absl::string_view format = format_value.NativeString(format_scratch);
  std::string result;
  result.reserve(format.size());
  int64_t arg_index = 0;
  CEL_ASSIGN_OR_RETURN(int64_t args_size, args.Size());
  for (int64_t i = 0; i < format.size(); ++i) {
    clause_scratch.clear();
    if (format[i] != '%') {
      result.push_back(format[i]);
      continue;
    }
    ++i;
    if (i >= format.size()) {
      return absl::InvalidArgumentError("unexpected end of format string");
    }
    if (format[i] == '%') {
      result.push_back('%');
      continue;
    }
    if (arg_index >= args_size) {
      return absl::InvalidArgumentError(
          absl::StrFormat("index %d out of range", arg_index));
    }
    CEL_ASSIGN_OR_RETURN(auto value, args.Get(arg_index++, descriptor_pool,
                                              message_factory, arena));
    CEL_ASSIGN_OR_RETURN(
        auto clause,
        ParseAndFormatClause(format.substr(i), value, descriptor_pool,
                             message_factory, arena, clause_scratch));
    absl::StrAppend(&result, clause.second);
    i += clause.first;
  }
  return StringValue(arena, std::move(result));
}

}  // namespace

absl::Status RegisterStringFormattingFunctions(FunctionRegistry& registry,
                                               const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue, ListValue>::
          CreateDescriptor("format", /*receiver_style=*/true),
      BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue, ListValue>::
          WrapFunction(
              [](const StringValue& format, const ListValue& args,
                 const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                 google::protobuf::MessageFactory* absl_nonnull message_factory,
                 google::protobuf::Arena* absl_nonnull arena) {
                return Format(format, args, descriptor_pool, message_factory,
                              arena);
              })));
  return absl::OkStatus();
}

}  // namespace cel::extensions
