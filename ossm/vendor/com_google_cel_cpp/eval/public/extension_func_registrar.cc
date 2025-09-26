#include "eval/public/extension_func_registrar.h"

#include <cstdint>
#include <string>
#include <utility>

#include "google/type/timeofday.pb.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::protobuf::Arena;

CelValue BetweenTs(Arena* arena, absl::Time time_stamp, absl::Time start,
                   absl::Time stop) {
  bool is_between = false;
  // check if timestamp paremeter is between start and stop parameters
  is_between = (start <= time_stamp) && (time_stamp < stop);
  return CelValue::CreateBool(is_between);
}

CelValue BetweenStr(Arena* arena, absl::Time time_stamp,
                    absl::string_view start, absl::string_view stop) {
  // convert start and stop into timestamps
  absl::Time start_ts;
  absl::Time stop_ts;
  // check if timestamp parameter is between start and stop -> call BetweenTs
  if (!absl::ParseTime(absl::RFC3339_full, start, &start_ts, nullptr) ||
      !absl::ParseTime(absl::RFC3339_full, stop, &stop_ts, nullptr)) {
    return CreateErrorValue(arena, "String to Timestamp conversion failed",
                            absl::StatusCode::kInvalidArgument);
  }

  return BetweenTs(arena, time_stamp, start_ts, stop_ts);
}

CelValue GetDateTz(Arena* arena, absl::Time time_stamp,
                   absl::TimeZone time_zone) {
  absl::Time ret_date;
  absl::CivilDay normalized_date;
  // convert absl time to civil day, which normalizes to midnight time,
  // convert the result to CivilSecond
  // convert CivilSecond from previous step back into absl::Time
  normalized_date = absl::ToCivilDay(time_stamp, time_zone);
  absl::CivilSecond normalized_date_cs(normalized_date);
  ret_date = absl::FromCivil(normalized_date_cs, time_zone);
  return CelValue::CreateTimestamp(ret_date);
}

CelValue GetDate(Arena* arena, absl::Time time_stamp,
                 absl::string_view time_zone) {
  absl::TimeZone time_zone_tz;
  // convert timezone from string to TimeZone
  if (!absl::LoadTimeZone(time_zone, &time_zone_tz)) {
    return CreateErrorValue(arena, "String to Timezone conversion failed",
                            absl::StatusCode::kInvalidArgument);
  }
  return GetDateTz(arena, time_stamp, time_zone_tz);
}

CelValue GetDateUTC(Arena* arena, absl::Time time_stamp) {
  absl::TimeZone time_zone = absl::UTCTimeZone();
  return GetDateTz(arena, time_stamp, time_zone);
}

CelValue GetTimeOfDayTz(Arena* arena, absl::Time time_stamp,
                        absl::TimeZone time_zone) {
  absl::CivilSecond date_civil_time =
      absl::ToCivilSecond(time_stamp, time_zone);
  google::type::TimeOfDay* tod_message =
      Arena::Create<google::type::TimeOfDay>(arena);

  tod_message->set_seconds(date_civil_time.second());
  tod_message->set_minutes(date_civil_time.minute());
  tod_message->set_hours(date_civil_time.hour());
  // transform into celvalue for return

  return CelProtoWrapper::CreateMessage(tod_message, arena);
}

CelValue GetTimeOfDay(Arena* arena, absl::Time time_stamp,
                      absl::string_view time_zone) {
  absl::TimeZone time_zonetz;

  if (!absl::LoadTimeZone(time_zone, &time_zonetz)) {
    return CreateErrorValue(arena, "String to Timezone conversion failed",
                            absl::StatusCode::kInvalidArgument);
  }

  return GetTimeOfDayTz(arena, time_stamp, time_zonetz);
}

CelValue GetTimeOfDayUTC(Arena* arena, absl::Time time_stamp) {
  absl::TimeZone utc = absl::UTCTimeZone();
  // call to helper function GetTimeOfDayTz
  // return value from helper
  return GetTimeOfDayTz(arena, time_stamp, utc);
}

int ToSeconds(const google::type::TimeOfDay* time_of_day) {
  int seconds = 0;

  seconds += time_of_day->hours() * 60 * 60;
  seconds += time_of_day->minutes() * 60;
  seconds += time_of_day->seconds();

  return seconds;
}

CelValue BetweenToD(Arena* arena, const google::protobuf::Message* time_of_day,
                    const google::protobuf::Message* start, const google::protobuf::Message* stop) {
  bool is_between;
  const google::type::TimeOfDay* time_of_day_tod =
      google::protobuf::DynamicCastMessage<const google::type::TimeOfDay>(time_of_day);
  const google::type::TimeOfDay* start_tod =
      google::protobuf::DynamicCastMessage<const google::type::TimeOfDay>(start);
  const google::type::TimeOfDay* stop_tod =
      google::protobuf::DynamicCastMessage<const google::type::TimeOfDay>(stop);

  if ((time_of_day_tod == nullptr) || (start_tod == nullptr) ||
      (stop_tod == nullptr)) {
    return CreateErrorValue(arena, "Message type downcast failed",
                            absl::StatusCode::kInvalidArgument);
  }
  // resolution for TimeOfDay in this function is 1 second
  int start_time = ToSeconds(start_tod);
  int stop_time = ToSeconds(stop_tod);
  int tod_time = ToSeconds(time_of_day_tod);

  is_between = (tod_time >= start_time) && (tod_time < stop_time);
  return CelValue::CreateBool(is_between);
}

CelValue BetweenToDStr(Arena* arena, const google::protobuf::Message* time_of_day,
                       absl::string_view start, absl::string_view stop) {
  std::string start_date_time = absl::StrCat("1970-01-01T", start, "+00:00");
  std::string stop_date_time = absl::StrCat("1970-01-01T", stop, "+00:00");
  absl::Time start_ts;
  absl::Time stop_ts;
  // format of time of day string: "HH:MM:SS"
  // Below we prepend a generic date string and append a generic timezone string
  // this generates a full timestamp string that can be parsed with ParseTime()

  if (!absl::ParseTime(absl::RFC3339_sec, start_date_time, absl::UTCTimeZone(),
                       &start_ts, nullptr) ||
      !absl::ParseTime(absl::RFC3339_sec, stop_date_time, absl::UTCTimeZone(),
                       &stop_ts, nullptr)) {
    return CreateErrorValue(arena, "String to Timestamp conversion failed",
                            absl::StatusCode::kInvalidArgument);
  }

  const google::protobuf::Message* start_msg =
      GetTimeOfDayUTC(arena, start_ts).MessageOrDie();
  const google::protobuf::Message* stop_msg =
      GetTimeOfDayUTC(arena, stop_ts).MessageOrDie();

  return BetweenToD(arena, time_of_day, start_msg, stop_msg);
}

absl::Status RegisterExtensionFunctions(CelFunctionRegistry* registry) {
  auto status = FunctionAdapter<CelValue, absl::Time, absl::Time, absl::Time>::
      CreateAndRegister(
          "between", true,
          [](Arena* arena, absl::Time ts, absl::Time start, absl::Time stop)
              -> CelValue { return BetweenTs(arena, ts, start, stop); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder,
                           CelValue::StringHolder>::
      CreateAndRegister(
          "between", true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder start,
             CelValue::StringHolder stop) -> CelValue {
            return BetweenStr(arena, ts, start.value(), stop.value());
          },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          "date", true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetDate(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      "date", true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDateUTC(arena, ts);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          "timeOfDay", true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetTimeOfDay(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      "timeOfDay", true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetTimeOfDayUTC(arena, ts);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, const google::protobuf::Message*,
                           const google::protobuf::Message*, const google::protobuf::Message*>::
      CreateAndRegister(
          "between", true,
          [](Arena* arena, const google::protobuf::Message* tod,
             const google::protobuf::Message* start,
             const google::protobuf::Message* stop) -> CelValue {
            return BetweenToD(arena, tod, start, stop);
          },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, const google::protobuf::Message*,
                           CelValue::StringHolder, CelValue::StringHolder>::
      CreateAndRegister(
          "between", true,
          [](Arena* arena, const google::protobuf::Message* tod,
             CelValue::StringHolder start,
             CelValue::StringHolder stop) -> CelValue {
            return BetweenToDStr(arena, tod, start.value(), stop.value());
          },
          registry);

  return status;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
