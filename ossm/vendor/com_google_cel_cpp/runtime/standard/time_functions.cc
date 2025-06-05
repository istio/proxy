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

#include "runtime/standard/time_functions.h"

#include <functional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"

namespace cel {
namespace {

// Timestamp
absl::Status FindTimeBreakdown(absl::Time timestamp, absl::string_view tz,
                               absl::TimeZone::CivilInfo* breakdown) {
  absl::TimeZone time_zone;

  // Early return if there is no timezone.
  if (tz.empty()) {
    *breakdown = time_zone.At(timestamp);
    return absl::OkStatus();
  }

  // Check to see whether the timezone is an IANA timezone.
  if (absl::LoadTimeZone(tz, &time_zone)) {
    *breakdown = time_zone.At(timestamp);
    return absl::OkStatus();
  }

  // Check for times of the format: [+-]HH:MM and convert them into durations
  // specified as [+-]HHhMMm.
  if (absl::StrContains(tz, ":")) {
    std::string dur = absl::StrCat(tz, "m");
    absl::StrReplaceAll({{":", "h"}}, &dur);
    absl::Duration d;
    if (absl::ParseDuration(dur, &d)) {
      timestamp += d;
      *breakdown = time_zone.At(timestamp);
      return absl::OkStatus();
    }
  }

  // Otherwise, error.
  return absl::InvalidArgumentError("Invalid timezone");
}

Value GetTimeBreakdownPart(
    ValueManager& value_factory, absl::Time timestamp, absl::string_view tz,
    const std::function<int64_t(const absl::TimeZone::CivilInfo&)>&
        extractor_func) {
  absl::TimeZone::CivilInfo breakdown;
  auto status = FindTimeBreakdown(timestamp, tz, &breakdown);

  if (!status.ok()) {
    return value_factory.CreateErrorValue(status);
  }

  return value_factory.CreateIntValue(extractor_func(breakdown));
}

Value GetFullYear(ValueManager& value_factory, absl::Time timestamp,
                  absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.year();
                              });
}

Value GetMonth(ValueManager& value_factory, absl::Time timestamp,
               absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.month() - 1;
                              });
}

Value GetDayOfYear(ValueManager& value_factory, absl::Time timestamp,
                   absl::string_view tz) {
  return GetTimeBreakdownPart(
      value_factory, timestamp, tz,
      [](const absl::TimeZone::CivilInfo& breakdown) {
        return absl::GetYearDay(absl::CivilDay(breakdown.cs)) - 1;
      });
}

Value GetDayOfMonth(ValueManager& value_factory, absl::Time timestamp,
                    absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.day() - 1;
                              });
}

Value GetDate(ValueManager& value_factory, absl::Time timestamp,
              absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.day();
                              });
}

Value GetDayOfWeek(ValueManager& value_factory, absl::Time timestamp,
                   absl::string_view tz) {
  return GetTimeBreakdownPart(
      value_factory, timestamp, tz,
      [](const absl::TimeZone::CivilInfo& breakdown) {
        absl::Weekday weekday = absl::GetWeekday(breakdown.cs);

        // get day of week from the date in UTC, zero-based, zero for Sunday,
        // based on GetDayOfWeek CEL function definition.
        int weekday_num = static_cast<int>(weekday);
        weekday_num = (weekday_num == 6) ? 0 : weekday_num + 1;
        return weekday_num;
      });
}

Value GetHours(ValueManager& value_factory, absl::Time timestamp,
               absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.hour();
                              });
}

Value GetMinutes(ValueManager& value_factory, absl::Time timestamp,
                 absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.minute();
                              });
}

Value GetSeconds(ValueManager& value_factory, absl::Time timestamp,
                 absl::string_view tz) {
  return GetTimeBreakdownPart(value_factory, timestamp, tz,
                              [](const absl::TimeZone::CivilInfo& breakdown) {
                                return breakdown.cs.second();
                              });
}

Value GetMilliseconds(ValueManager& value_factory, absl::Time timestamp,
                      absl::string_view tz) {
  return GetTimeBreakdownPart(
      value_factory, timestamp, tz,
      [](const absl::TimeZone::CivilInfo& breakdown) {
        return absl::ToInt64Milliseconds(breakdown.subsecond);
      });
}

absl::Status RegisterTimestampFunctions(FunctionRegistry& registry,
                                        const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kFullYear, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetFullYear(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kFullYear, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetFullYear(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kMonth, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetMonth(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(builtin::kMonth,
                                                                true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetMonth(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kDayOfYear, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetDayOfYear(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kDayOfYear, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetDayOfYear(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kDayOfMonth, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetDayOfMonth(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kDayOfMonth, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetDayOfMonth(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kDate, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetDate(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(builtin::kDate,
                                                                true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetDate(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kDayOfWeek, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetDayOfWeek(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kDayOfWeek, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetDayOfWeek(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kHours, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetHours(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(builtin::kHours,
                                                                true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetHours(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kMinutes, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetMinutes(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kMinutes, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetMinutes(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kSeconds, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetSeconds(value_factory, ts, tz.ToString());
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kSeconds, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetSeconds(value_factory, ts, "");
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          CreateDescriptor(builtin::kMilliseconds, true),
      BinaryFunctionAdapter<Value, absl::Time, const StringValue&>::
          WrapFunction([](ValueManager& value_factory, absl::Time ts,
                          const StringValue& tz) -> Value {
            return GetMilliseconds(value_factory, ts, tz.ToString());
          })));

  return registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          builtin::kMilliseconds, true),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time ts) -> Value {
            return GetMilliseconds(value_factory, ts, "");
          }));
}

absl::Status RegisterCheckedTimeArithmeticFunctions(
    FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Time, absl::Duration>::
          WrapFunction([](ValueManager& value_factory, absl::Time t1,
                          absl::Duration d2) -> absl::StatusOr<Value> {
            auto sum = cel::internal::CheckedAdd(t1, d2);
            if (!sum.ok()) {
              return value_factory.CreateErrorValue(sum.status());
            }
            return value_factory.CreateTimestampValue(*sum);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Duration,
                            absl::Time>::CreateDescriptor(builtin::kAdd, false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Duration, absl::Time>::
          WrapFunction([](ValueManager& value_factory, absl::Duration d2,
                          absl::Time t1) -> absl::StatusOr<Value> {
            auto sum = cel::internal::CheckedAdd(t1, d2);
            if (!sum.ok()) {
              return value_factory.CreateErrorValue(sum.status());
            }
            return value_factory.CreateTimestampValue(*sum);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Duration,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Duration,
                            absl::Duration>::
          WrapFunction([](ValueManager& value_factory, absl::Duration d1,
                          absl::Duration d2) -> absl::StatusOr<Value> {
            auto sum = cel::internal::CheckedAdd(d1, d2);
            if (!sum.ok()) {
              return value_factory.CreateErrorValue(sum.status());
            }
            return value_factory.CreateDurationValue(*sum);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Time, absl::Duration>::
          CreateDescriptor(builtin::kSubtract, false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Time, absl::Duration>::
          WrapFunction([](ValueManager& value_factory, absl::Time t1,
                          absl::Duration d2) -> absl::StatusOr<Value> {
            auto diff = cel::internal::CheckedSub(t1, d2);
            if (!diff.ok()) {
              return value_factory.CreateErrorValue(diff.status());
            }
            return value_factory.CreateTimestampValue(*diff);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Time,
                            absl::Time>::CreateDescriptor(builtin::kSubtract,
                                                          false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Time, absl::Time>::
          WrapFunction([](ValueManager& value_factory, absl::Time t1,
                          absl::Time t2) -> absl::StatusOr<Value> {
            auto diff = cel::internal::CheckedSub(t1, t2);
            if (!diff.ok()) {
              return value_factory.CreateErrorValue(diff.status());
            }
            return value_factory.CreateDurationValue(*diff);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Value>, absl::Duration,
          absl::Duration>::CreateDescriptor(builtin::kSubtract, false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Duration,
                            absl::Duration>::
          WrapFunction([](ValueManager& value_factory, absl::Duration d1,
                          absl::Duration d2) -> absl::StatusOr<Value> {
            auto diff = cel::internal::CheckedSub(d1, d2);
            if (!diff.ok()) {
              return value_factory.CreateErrorValue(diff.status());
            }
            return value_factory.CreateDurationValue(*diff);
          })));

  return absl::OkStatus();
}

absl::Status RegisterUncheckedTimeArithmeticFunctions(
    FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<Value, absl::Time, absl::Duration>::WrapFunction(
          [](ValueManager& value_factory, absl::Time t1,
             absl::Duration d2) -> Value {
            return value_factory.CreateUncheckedTimestampValue(t1 + d2);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Duration,
                            absl::Time>::CreateDescriptor(builtin::kAdd, false),
      BinaryFunctionAdapter<Value, absl::Duration, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Duration d2,
             absl::Time t1) -> Value {
            return value_factory.CreateUncheckedTimestampValue(t1 + d2);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Duration,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<Value, absl::Duration, absl::Duration>::
          WrapFunction([](ValueManager& value_factory, absl::Duration d1,
                          absl::Duration d2) -> Value {
            return value_factory.CreateUncheckedDurationValue(d1 + d2);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, absl::Duration>::
          CreateDescriptor(builtin::kSubtract, false),

      BinaryFunctionAdapter<Value, absl::Time, absl::Duration>::WrapFunction(

          [](ValueManager& value_factory, absl::Time t1,
             absl::Duration d2) -> Value {
            return value_factory.CreateUncheckedTimestampValue(t1 - d2);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Time, absl::Time>::CreateDescriptor(
          builtin::kSubtract, false),
      BinaryFunctionAdapter<Value, absl::Time, absl::Time>::WrapFunction(

          [](ValueManager& value_factory, absl::Time t1,
             absl::Time t2) -> Value {
            return value_factory.CreateUncheckedDurationValue(t1 - t2);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, absl::Duration, absl::Duration>::
          CreateDescriptor(builtin::kSubtract, false),
      BinaryFunctionAdapter<Value, absl::Duration, absl::Duration>::
          WrapFunction([](ValueManager& value_factory, absl::Duration d1,
                          absl::Duration d2) -> Value {
            return value_factory.CreateUncheckedDurationValue(d1 - d2);
          })));

  return absl::OkStatus();
}

absl::Status RegisterDurationFunctions(FunctionRegistry& registry) {
  // duration breakdown accessor functions
  using DurationAccessorFunction =
      UnaryFunctionAdapter<int64_t, absl::Duration>;
  CEL_RETURN_IF_ERROR(registry.Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kHours, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueManager&, absl::Duration d) -> int64_t {
            return absl::ToInt64Hours(d);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kMinutes, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueManager&, absl::Duration d) -> int64_t {
            return absl::ToInt64Minutes(d);
          })));

  CEL_RETURN_IF_ERROR(registry.Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kSeconds, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueManager&, absl::Duration d) -> int64_t {
            return absl::ToInt64Seconds(d);
          })));

  return registry.Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kMilliseconds, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueManager&, absl::Duration d) -> int64_t {
            constexpr int64_t millis_per_second = 1000L;
            return absl::ToInt64Milliseconds(d) % millis_per_second;
          }));
}

}  // namespace

absl::Status RegisterTimeFunctions(FunctionRegistry& registry,
                                   const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterTimestampFunctions(registry, options));
  CEL_RETURN_IF_ERROR(RegisterDurationFunctions(registry));

  // Special arithmetic operators for Timestamp and Duration
  // TODO: deprecate unchecked time math functions when clients no
  // longer depend on them.
  if (options.enable_timestamp_duration_overflow_errors) {
    return RegisterCheckedTimeArithmeticFunctions(registry);
  }

  return RegisterUncheckedTimeArithmeticFunctions(registry);
}

}  // namespace cel
