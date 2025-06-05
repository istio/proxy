// Copyright 2023 Google LLC
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

#include "runtime/standard/type_conversion_functions.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

using ::cel::internal::EncodeDurationToJson;
using ::cel::internal::EncodeTimestampToJson;
using ::cel::internal::MaxTimestamp;

// Time representing `9999-12-31T23:59:59.999999999Z`.
const absl::Time kMaxTime = MaxTimestamp();

absl::Status RegisterBoolConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions&) {
  // bool -> bool
  return UnaryFunctionAdapter<bool, bool>::RegisterGlobalOverload(
      cel::builtin::kBool, [](ValueManager&, bool v) { return v; }, registry);
}

absl::Status RegisterIntConversionFunctions(FunctionRegistry& registry,
                                            const RuntimeOptions&) {
  // bool -> int
  absl::Status status =
      UnaryFunctionAdapter<int64_t, bool>::RegisterGlobalOverload(
          cel::builtin::kInt,
          [](ValueManager&, bool v) { return static_cast<int64_t>(v); },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // double -> int
  status = UnaryFunctionAdapter<Value, double>::RegisterGlobalOverload(
      cel::builtin::kInt,
      [](ValueManager& value_factory, double v) -> Value {
        auto conv = cel::internal::CheckedDoubleToInt64(v);
        if (!conv.ok()) {
          return value_factory.CreateErrorValue(conv.status());
        }
        return value_factory.CreateIntValue(*conv);
      },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // int -> int
  status = UnaryFunctionAdapter<int64_t, int64_t>::RegisterGlobalOverload(
      cel::builtin::kInt, [](ValueManager&, int64_t v) { return v; }, registry);
  CEL_RETURN_IF_ERROR(status);

  // string -> int
  status =
      UnaryFunctionAdapter<Value, const StringValue&>::RegisterGlobalOverload(
          cel::builtin::kInt,
          [](ValueManager& value_factory, const StringValue& s) -> Value {
            int64_t result;
            if (!absl::SimpleAtoi(s.ToString(), &result)) {
              return value_factory.CreateErrorValue(
                  absl::InvalidArgumentError("cannot convert string to int"));
            }
            return value_factory.CreateIntValue(result);
          },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // time -> int
  status = UnaryFunctionAdapter<int64_t, absl::Time>::RegisterGlobalOverload(
      cel::builtin::kInt,
      [](ValueManager&, absl::Time t) { return absl::ToUnixSeconds(t); },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // uint -> int
  return UnaryFunctionAdapter<Value, uint64_t>::RegisterGlobalOverload(
      cel::builtin::kInt,
      [](ValueManager& value_factory, uint64_t v) -> Value {
        auto conv = cel::internal::CheckedUint64ToInt64(v);
        if (!conv.ok()) {
          return value_factory.CreateErrorValue(conv.status());
        }
        return value_factory.CreateIntValue(*conv);
      },
      registry);
}

absl::Status RegisterStringConversionFunctions(FunctionRegistry& registry,
                                               const RuntimeOptions& options) {
  // May be optionally disabled to reduce potential allocs.
  if (!options.enable_string_conversion) {
    return absl::OkStatus();
  }

  absl::Status status =
      UnaryFunctionAdapter<Value, const BytesValue&>::RegisterGlobalOverload(
          cel::builtin::kString,

          [](ValueManager& value_factory, const BytesValue& value) -> Value {
            auto handle_or = value_factory.CreateStringValue(value.ToString());
            if (!handle_or.ok()) {
              return value_factory.CreateErrorValue(handle_or.status());
            }
            return *handle_or;
          },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // double -> string
  status = UnaryFunctionAdapter<StringValue, double>::RegisterGlobalOverload(
      cel::builtin::kString,
      [](ValueManager& value_factory, double value) -> StringValue {
        return value_factory.CreateUncheckedStringValue(absl::StrCat(value));
      },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // int -> string
  status = UnaryFunctionAdapter<StringValue, int64_t>::RegisterGlobalOverload(
      cel::builtin::kString,
      [](ValueManager& value_factory, int64_t value) -> StringValue {
        return value_factory.CreateUncheckedStringValue(absl::StrCat(value));
      },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // string -> string
  status =
      UnaryFunctionAdapter<StringValue, StringValue>::RegisterGlobalOverload(
          cel::builtin::kString,
          [](ValueManager&, StringValue value) -> StringValue { return value; },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // uint -> string
  status = UnaryFunctionAdapter<StringValue, uint64_t>::RegisterGlobalOverload(
      cel::builtin::kString,
      [](ValueManager& value_factory, uint64_t value) -> StringValue {
        return value_factory.CreateUncheckedStringValue(absl::StrCat(value));
      },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // duration -> string
  status = UnaryFunctionAdapter<Value, absl::Duration>::RegisterGlobalOverload(
      cel::builtin::kString,
      [](ValueManager& value_factory, absl::Duration value) -> Value {
        auto encode = EncodeDurationToJson(value);
        if (!encode.ok()) {
          return value_factory.CreateErrorValue(encode.status());
        }
        return value_factory.CreateUncheckedStringValue(*encode);
      },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // timestamp -> string
  return UnaryFunctionAdapter<Value, absl::Time>::RegisterGlobalOverload(
      cel::builtin::kString,
      [](ValueManager& value_factory, absl::Time value) -> Value {
        auto encode = EncodeTimestampToJson(value);
        if (!encode.ok()) {
          return value_factory.CreateErrorValue(encode.status());
        }
        return value_factory.CreateUncheckedStringValue(*encode);
      },
      registry);
}

absl::Status RegisterUintConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions&) {
  // double -> uint
  absl::Status status =
      UnaryFunctionAdapter<Value, double>::RegisterGlobalOverload(
          cel::builtin::kUint,
          [](ValueManager& value_factory, double v) -> Value {
            auto conv = cel::internal::CheckedDoubleToUint64(v);
            if (!conv.ok()) {
              return value_factory.CreateErrorValue(conv.status());
            }
            return value_factory.CreateUintValue(*conv);
          },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // int -> uint
  status = UnaryFunctionAdapter<Value, int64_t>::RegisterGlobalOverload(
      cel::builtin::kUint,
      [](ValueManager& value_factory, int64_t v) -> Value {
        auto conv = cel::internal::CheckedInt64ToUint64(v);
        if (!conv.ok()) {
          return value_factory.CreateErrorValue(conv.status());
        }
        return value_factory.CreateUintValue(*conv);
      },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // string -> uint
  status =
      UnaryFunctionAdapter<Value, const StringValue&>::RegisterGlobalOverload(
          cel::builtin::kUint,
          [](ValueManager& value_factory, const StringValue& s) -> Value {
            uint64_t result;
            if (!absl::SimpleAtoi(s.ToString(), &result)) {
              return value_factory.CreateErrorValue(
                  absl::InvalidArgumentError("doesn't convert to a string"));
            }
            return value_factory.CreateUintValue(result);
          },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // uint -> uint
  return UnaryFunctionAdapter<uint64_t, uint64_t>::RegisterGlobalOverload(
      cel::builtin::kUint, [](ValueManager&, uint64_t v) { return v; },
      registry);
}

absl::Status RegisterBytesConversionFunctions(FunctionRegistry& registry,
                                              const RuntimeOptions&) {
  // bytes -> bytes
  absl::Status status =
      UnaryFunctionAdapter<BytesValue, BytesValue>::RegisterGlobalOverload(
          cel::builtin::kBytes,

          [](ValueManager&, BytesValue value) -> BytesValue { return value; },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // string -> bytes
  return UnaryFunctionAdapter<absl::StatusOr<BytesValue>, const StringValue&>::
      RegisterGlobalOverload(
          cel::builtin::kBytes,
          [](ValueManager& value_factory, const StringValue& value) {
            return value_factory.CreateBytesValue(value.ToString());
          },
          registry);
}

absl::Status RegisterDoubleConversionFunctions(FunctionRegistry& registry,
                                               const RuntimeOptions&) {
  // double -> double
  absl::Status status =
      UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          cel::builtin::kDouble, [](ValueManager&, double v) { return v; },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // int -> double
  status = UnaryFunctionAdapter<double, int64_t>::RegisterGlobalOverload(
      cel::builtin::kDouble,
      [](ValueManager&, int64_t v) { return static_cast<double>(v); },
      registry);
  CEL_RETURN_IF_ERROR(status);

  // string -> double
  status =
      UnaryFunctionAdapter<Value, const StringValue&>::RegisterGlobalOverload(
          cel::builtin::kDouble,
          [](ValueManager& value_factory, const StringValue& s) -> Value {
            double result;
            if (absl::SimpleAtod(s.ToString(), &result)) {
              return value_factory.CreateDoubleValue(result);
            } else {
              return value_factory.CreateErrorValue(absl::InvalidArgumentError(
                  "cannot convert string to double"));
            }
          },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // uint -> double
  return UnaryFunctionAdapter<double, uint64_t>::RegisterGlobalOverload(
      cel::builtin::kDouble,
      [](ValueManager&, uint64_t v) { return static_cast<double>(v); },
      registry);
}

Value CreateDurationFromString(ValueManager& value_factory,
                               const StringValue& dur_str) {
  absl::Duration d;
  if (!absl::ParseDuration(dur_str.ToString(), &d)) {
    return value_factory.CreateErrorValue(
        absl::InvalidArgumentError("String to Duration conversion failed"));
  }

  auto duration = value_factory.CreateDurationValue(d);

  if (!duration.ok()) {
    return value_factory.CreateErrorValue(duration.status());
  }

  return *duration;
}

absl::Status RegisterTimeConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions& options) {
  // duration() conversion from string.
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, const StringValue&>::RegisterGlobalOverload(
          cel::builtin::kDuration, CreateDurationFromString, registry)));

  // timestamp conversion from int.
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, int64_t>::RegisterGlobalOverload(
          cel::builtin::kTimestamp,
          [](ValueManager& value_factory, int64_t epoch_seconds) -> Value {
            return value_factory.CreateUncheckedTimestampValue(
                absl::FromUnixSeconds(epoch_seconds));
          },
          registry)));

  // timestamp -> timestamp
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, absl::Time>::RegisterGlobalOverload(
          cel::builtin::kTimestamp,
          [](ValueManager&, absl::Time value) -> Value {
            return TimestampValue(value);
          },
          registry)));

  // duration -> duration
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, absl::Duration>::RegisterGlobalOverload(
          cel::builtin::kDuration,
          [](ValueManager&, absl::Duration value) -> Value {
            return DurationValue(value);
          },
          registry)));

  // timestamp() conversion from string.
  bool enable_timestamp_duration_overflow_errors =
      options.enable_timestamp_duration_overflow_errors;
  return UnaryFunctionAdapter<Value, const StringValue&>::
      RegisterGlobalOverload(
          cel::builtin::kTimestamp,
          [=](ValueManager& value_factory,
              const StringValue& time_str) -> Value {
            absl::Time ts;
            if (!absl::ParseTime(absl::RFC3339_full, time_str.ToString(), &ts,
                                 nullptr)) {
              return value_factory.CreateErrorValue(absl::InvalidArgumentError(
                  "String to Timestamp conversion failed"));
            }
            if (enable_timestamp_duration_overflow_errors) {
              if (ts < absl::UniversalEpoch() || ts > kMaxTime) {
                return value_factory.CreateErrorValue(
                    absl::OutOfRangeError("timestamp overflow"));
              }
            }
            return value_factory.CreateUncheckedTimestampValue(ts);
          },
          registry);
}

}  // namespace

absl::Status RegisterTypeConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterBoolConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterBytesConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterDoubleConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterIntConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterStringConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterUintConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterTimeConversionFunctions(registry, options));

  // dyn() identity function.
  // TODO: strip dyn() function references at type-check time.
  absl::Status status =
      UnaryFunctionAdapter<Value, const Value&>::RegisterGlobalOverload(
          cel::builtin::kDyn,
          [](ValueManager&, const Value& value) -> Value { return value; },
          registry);
  CEL_RETURN_IF_ERROR(status);

  // type(dyn) -> type
  return UnaryFunctionAdapter<Value, const Value&>::RegisterGlobalOverload(
      cel::builtin::kType,
      [](ValueManager& factory, const Value& value) {
        return factory.CreateTypeValue(value.GetRuntimeType());
      },
      registry);
}

}  // namespace cel
