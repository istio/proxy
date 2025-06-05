#include <string>
#include <vector>

#include "google/type/timeofday.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/time_util.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/extension_func_registrar.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {
using google::protobuf::Arena;

static const int kNanosPerSecond = 1000000000;

class ExtensionTest : public ::testing::Test {
 protected:
  ExtensionTest() {}

  void SetUp() override {
    ASSERT_OK(RegisterBuiltinFunctions(&registry_));
    ASSERT_OK(RegisterExtensionFunctions(&registry_));
  }

  // Helper method to test string startsWith() function
  void TestStringInclusion(absl::string_view func_name,
                           const std::vector<bool>& call_style,
                           const std::string& test_string,
                           const std::string& included, bool result) {
    std::vector<bool> call_styles = {true, false};

    for (auto call_style : call_styles) {
      auto functions = registry_.FindOverloads(
          func_name, call_style,
          {CelValue::Type::kString, CelValue::Type::kString});
      ASSERT_EQ(functions.size(), 1);

      auto func = functions[0];

      std::vector<CelValue> args = {CelValue::CreateString(&test_string),
                                    CelValue::CreateString(&included)};
      CelValue result_value = CelValue::CreateNull();
      google::protobuf::Arena arena;
      absl::Span<CelValue> arg_span(&args[0], args.size());
      auto status = func->Evaluate(arg_span, &result_value, &arena);

      ASSERT_OK(status);
      ASSERT_TRUE(result_value.IsBool());
      ASSERT_EQ(result_value.BoolOrDie(), result);
    }
  }

  void TestStringStartsWith(const std::string& test_string,
                            const std::string& prefix, bool result) {
    TestStringInclusion("startsWith", {true, false}, test_string, prefix,
                        result);
  }

  void TestStringEndsWith(const std::string& test_string,
                          const std::string& prefix, bool result) {
    TestStringInclusion("endsWith", {true, false}, test_string, prefix, result);
  }

  // Helper method to test timestamp() function
  void PerformTimestampConversion(Arena* arena, const std::string& ts_str,
                                  CelValue* result) {
    auto functions =
        registry_.FindOverloads("timestamp", false, {CelValue::Type::kString});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateString(&ts_str)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformBetweenTest(Arena* arena, absl::Time time_stamp,
                          absl::Time start_ts, absl::Time stop_ts,
                          CelValue* result) {
    auto functions = registry_.FindOverloads(
        "between", true,
        {CelValue::Type::kTimestamp, CelValue::Type::kTimestamp,
         CelValue::Type::kTimestamp});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateTimestamp(time_stamp),
                                  CelValue::CreateTimestamp(start_ts),
                                  CelValue::CreateTimestamp(stop_ts)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformBetweenStrTest(Arena* arena, absl::Time time_stamp,
                             std::string* start, std::string* stop,
                             CelValue* result) {
    auto functions = registry_.FindOverloads(
        "between", true,
        {CelValue::Type::kTimestamp, CelValue::Type::kString,
         CelValue::Type::kString});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateTimestamp(time_stamp),
                                  CelValue::CreateString(start),
                                  CelValue::CreateString(stop)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformGetDateTest(Arena* arena, absl::Time time_stamp,
                          std::string* time_zone, CelValue* result) {
    auto functions = registry_.FindOverloads(
        "date", true, {CelValue::Type::kTimestamp, CelValue::Type::kString});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateTimestamp(time_stamp),
                                  CelValue::CreateString(time_zone)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformGetDateUTCTest(Arena* arena, absl::Time time_stamp,
                             CelValue* result) {
    auto functions =
        registry_.FindOverloads("date", true, {CelValue::Type::kTimestamp});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateTimestamp(time_stamp)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformGetTimeOfDayTest(Arena* arena, absl::Time time_stamp,
                               std::string* time_zone, CelValue* result) {
    auto functions = registry_.FindOverloads(
        "timeOfDay", true,
        {CelValue::Type::kTimestamp, CelValue::Type::kString});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateTimestamp(time_stamp),
                                  CelValue::CreateString(time_zone)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformGetTimeOfDayUTCTest(Arena* arena, absl::Time time_stamp,
                                  CelValue* result) {
    auto functions = registry_.FindOverloads("timeOfDay", true,
                                             {CelValue::Type::kTimestamp});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateTimestamp(time_stamp)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformBetweenToDTest(Arena* arena, const google::protobuf::Message* time_of_day,
                             const google::protobuf::Message* start,
                             const google::protobuf::Message* stop, CelValue* result) {
    auto functions = registry_.FindOverloads(
        "between", true,
        {CelValue::Type::kMessage, CelValue::Type::kMessage,
         CelValue::Type::kMessage});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {
        CelProtoWrapper::CreateMessage(time_of_day, arena),
        CelProtoWrapper::CreateMessage(start, arena),
        CelProtoWrapper::CreateMessage(stop, arena)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  void PerformBetweenToDStrTest(Arena* arena,
                                const google::protobuf::Message* time_of_day,
                                std::string* start, std::string* stop,
                                CelValue* result) {
    auto functions = registry_.FindOverloads(
        "between", true,
        {CelValue::Type::kMessage, CelValue::Type::kString,
         CelValue::Type::kString});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {
        CelProtoWrapper::CreateMessage(time_of_day, arena),
        CelValue::CreateString(start), CelValue::CreateString(stop)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  // Helper method to test duration() function
  void PerformDurationConversion(Arena* arena, const std::string& ts_str,
                                 CelValue* result) {
    auto functions =
        registry_.FindOverloads("duration", false, {CelValue::Type::kString});
    ASSERT_EQ(functions.size(), 1);

    auto func = functions[0];

    std::vector<CelValue> args = {CelValue::CreateString(&ts_str)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);

    ASSERT_OK(status);
  }

  // Function registry object
  CelFunctionRegistry registry_;
  Arena arena_;
};

// Test string startsWith() function.
TEST_F(ExtensionTest, TestStartsWithFunction) {
  // Empty string, non-empty prefix - never matches.
  EXPECT_NO_FATAL_FAILURE(TestStringStartsWith("", "p", false));
  // Prefix of 0 length - always matches.
  EXPECT_NO_FATAL_FAILURE(TestStringStartsWith("", "", true));
  EXPECT_NO_FATAL_FAILURE(TestStringStartsWith("prefixedString", "", true));
  // Non-empty matching prefix.
  EXPECT_NO_FATAL_FAILURE(
      TestStringStartsWith("prefixedString", "prefix", true));
  // Non-empty mismatching prefix.
  EXPECT_NO_FATAL_FAILURE(TestStringStartsWith("prefixedString", "x", false));
  EXPECT_NO_FATAL_FAILURE(
      TestStringStartsWith("prefixedString", "prefixedString1", false));
}

// Test string startsWith() function.
TEST_F(ExtensionTest, TestEndsWithFunction) {
  // Empty string, non-empty postfix - never matches.
  EXPECT_NO_FATAL_FAILURE(TestStringEndsWith("", "p", false));
  // Postfix of 0 length - always matches.
  EXPECT_NO_FATAL_FAILURE(TestStringEndsWith("", "", true));
  EXPECT_NO_FATAL_FAILURE(TestStringEndsWith("postfixedString", "", true));
  // Non-empty matching postfix.
  EXPECT_NO_FATAL_FAILURE(
      TestStringEndsWith("postfixedString", "String", true));
  // Non-empty mismatching post.
  EXPECT_NO_FATAL_FAILURE(TestStringEndsWith("postfixedString", "x", false));
  EXPECT_NO_FATAL_FAILURE(
      TestStringEndsWith("postfixedString", "1postfixedString", false));
}

// Test timestamp conversion function.
TEST_F(ExtensionTest, TestTimestampFromString) {
  CelValue result = CelValue::CreateNull();

  Arena arena;

  // Valid timestamp - no fractions of seconds.
  EXPECT_NO_FATAL_FAILURE(
      PerformTimestampConversion(&arena, "2000-01-01T00:00:00Z", &result));
  ASSERT_TRUE(result.IsTimestamp());

  auto ts = result.TimestampOrDie();
  ASSERT_EQ(absl::ToUnixSeconds(ts), 946684800L);
  ASSERT_EQ(absl::ToUnixNanos(ts), 946684800L * kNanosPerSecond);

  // Valid timestamp - with nanoseconds.
  EXPECT_NO_FATAL_FAILURE(
      PerformTimestampConversion(&arena, "2000-01-01T00:00:00.212Z", &result));
  ASSERT_TRUE(result.IsTimestamp());

  ts = result.TimestampOrDie();
  ASSERT_EQ(absl::ToUnixSeconds(ts), 946684800L);
  ASSERT_EQ(absl::ToUnixNanos(ts), 946684800L * kNanosPerSecond + 212000000);

  // Valid timestamp - with timezone.
  EXPECT_NO_FATAL_FAILURE(PerformTimestampConversion(
      &arena, "2000-01-01T00:00:00.212-01:00", &result));
  ASSERT_TRUE(result.IsTimestamp());

  ts = result.TimestampOrDie();
  ASSERT_EQ(absl::ToUnixSeconds(ts), 946688400L);
  ASSERT_EQ(absl::ToUnixNanos(ts), 946688400L * kNanosPerSecond + 212000000);

  // Invalid timestamp - empty string.
  EXPECT_NO_FATAL_FAILURE(PerformTimestampConversion(&arena, "", &result));
  ASSERT_TRUE(result.IsError());
  ASSERT_EQ(result.ErrorOrDie()->code(), absl::StatusCode::kInvalidArgument);

  // Invalid timestamp.
  EXPECT_NO_FATAL_FAILURE(
      PerformTimestampConversion(&arena, "2000-01-01TT00:00:00Z", &result));
  ASSERT_TRUE(result.IsError());
}

// Test duration conversion function.
TEST_F(ExtensionTest, TestDurationFromString) {
  CelValue result = CelValue::CreateNull();

  Arena arena;

  // Valid duration - no fractions of seconds.
  EXPECT_NO_FATAL_FAILURE(PerformDurationConversion(&arena, "1354s", &result));
  ASSERT_TRUE(result.IsDuration());

  auto d = result.DurationOrDie();
  ASSERT_EQ(absl::ToInt64Seconds(d), 1354L);
  ASSERT_EQ(absl::ToInt64Nanoseconds(d), 1354L * kNanosPerSecond);

  // Valid duration - with nanoseconds.
  EXPECT_NO_FATAL_FAILURE(PerformDurationConversion(&arena, "15.11s", &result));
  ASSERT_TRUE(result.IsDuration());

  d = result.DurationOrDie();
  ASSERT_EQ(absl::ToInt64Seconds(d), 15L);
  ASSERT_EQ(absl::ToInt64Nanoseconds(d), 15L * kNanosPerSecond + 110000000L);

  // Invalid duration - empty string.
  EXPECT_NO_FATAL_FAILURE(PerformDurationConversion(&arena, "", &result));
  ASSERT_TRUE(result.IsError());
  ASSERT_EQ(result.ErrorOrDie()->code(), absl::StatusCode::kInvalidArgument);

  // Invalid duration.
  EXPECT_NO_FATAL_FAILURE(PerformDurationConversion(&arena, "100", &result));
  ASSERT_TRUE(result.IsError());
}

TEST_F(ExtensionTest, TestBetweenTs) {
  absl::Time time_1;
  absl::Time time_2;
  absl::Time time_3;
  std::string time_stampstr = "1997-07-16T19:50:30.45+01:00";
  std::string time_start = "1997-07-16T19:20:30.45+01:00";
  std::string time_stop = "1997-07-16T20:20:30.45+01:00";
  Arena arena;
  CelValue result;

  absl::ParseTime(absl::RFC3339_full, time_stampstr, &time_2, nullptr);
  absl::ParseTime(absl::RFC3339_full, time_start, &time_1, nullptr);
  absl::ParseTime(absl::RFC3339_full, time_stop, &time_3, nullptr);

  PerformBetweenTest(&arena, time_2, time_1, time_3, &result);
  ASSERT_EQ(result.BoolOrDie(), true);
  PerformBetweenTest(&arena, time_1, time_2, time_3, &result);
  ASSERT_EQ(result.BoolOrDie(), false);
  PerformBetweenTest(&arena, time_1, time_1, time_3, &result);
  ASSERT_EQ(result.BoolOrDie(), true);
  PerformBetweenTest(&arena, time_3, time_1, time_2, &result);
  ASSERT_EQ(result.BoolOrDie(), false);
  PerformBetweenTest(&arena, time_3, time_1, time_3, &result);
  ASSERT_EQ(result.BoolOrDie(), false);
}

TEST_F(ExtensionTest, TestBetweenStr) {
  Arena arena;
  absl::Time time_stamp;
  CelValue result;
  std::string time_stampstr = "1997-07-16T19:50:30.45+01:00";
  std::string time_start = "1997-07-16T19:20:30.45+01:00";
  std::string time_stop = "1997-07-16T20:20:30.45+01:00";

  absl::ParseTime(absl::RFC3339_full, time_stampstr, &time_stamp, nullptr);
  PerformBetweenStrTest(&arena, time_stamp, &time_start, &time_stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  absl::ParseTime(absl::RFC3339_full, time_start, &time_stamp, nullptr);
  PerformBetweenStrTest(&arena, time_stamp, &time_start, &time_stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  absl::ParseTime(absl::RFC3339_full, time_stop, &time_stamp, nullptr);
  PerformBetweenStrTest(&arena, time_stamp, &time_start, &time_stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);

  time_stampstr = "1997-07-16T18:20:30.45+01:00";
  absl::ParseTime(absl::RFC3339_full, time_stampstr, &time_stamp, nullptr);
  PerformBetweenStrTest(&arena, time_stamp, &time_start, &time_stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);

  time_stampstr = "1997-07-16T21:20:30.45+01:00";
  absl::ParseTime(absl::RFC3339_full, time_stampstr, &time_stamp, nullptr);
  PerformBetweenStrTest(&arena, time_stamp, &time_start, &time_stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);
}

TEST_F(ExtensionTest, TestGetDate) {
  Arena arena;
  CelValue result;
  absl::CivilSecond date(2015, 2, 3, 4, 5, 6);
  absl::CivilSecond normal_date(2015, 2, 3);
  absl::TimeZone time_zone;
  std::string time_zonestr = "America/Los_Angeles";
  absl::LoadTimeZone(time_zonestr, &time_zone);

  absl::Time expected_val = absl::FromCivil(normal_date, time_zone);
  absl::Time input_val = absl::FromCivil(date, time_zone);

  PerformGetDateTest(&arena, input_val, &time_zonestr, &result);
  ASSERT_EQ(result.TimestampOrDie(), expected_val);
}

TEST_F(ExtensionTest, TestGetDateUTC) {
  Arena arena;
  CelValue result;
  absl::CivilSecond date(2015, 2, 3, 4, 5, 6);
  absl::CivilSecond normal_date(2015, 2, 3);
  absl::TimeZone time_zone = absl::UTCTimeZone();

  absl::Time expected_val = absl::FromCivil(normal_date, time_zone);
  absl::Time input_val = absl::FromCivil(date, time_zone);

  PerformGetDateUTCTest(&arena, input_val, &result);
  ASSERT_EQ(result.TimestampOrDie(), expected_val);
}

TEST_F(ExtensionTest, TestGetTimeOfDay) {
  Arena arena;
  CelValue result;
  absl::CivilSecond date(2015, 2, 3, 4, 5, 6);
  absl::TimeZone time_zone;
  std::string time_zonestr = "America/Los_Angeles";
  google::type::TimeOfDay* tod_message =
      Arena::Create<google::type::TimeOfDay>(&arena);

  absl::LoadTimeZone(time_zonestr, &time_zone);
  absl::Time input_val = absl::FromCivil(date, time_zone);

  tod_message->set_seconds(date.second());
  tod_message->set_minutes(date.minute());
  tod_message->set_hours(date.hour());

  PerformGetTimeOfDayTest(&arena, input_val, &time_zonestr, &result);
  const google::type::TimeOfDay* time_of_day_tod =
      google::protobuf::DynamicCastMessage<const google::type::TimeOfDay>(
          result.MessageOrDie());

  ASSERT_EQ(time_of_day_tod->seconds(), tod_message->seconds());
  ASSERT_EQ(time_of_day_tod->minutes(), tod_message->minutes());
  ASSERT_EQ(time_of_day_tod->hours(), tod_message->hours());
}

TEST_F(ExtensionTest, TestGetTimeOfDayUTC) {
  Arena arena;
  CelValue result;
  absl::TimeZone time_zone = absl::UTCTimeZone();
  absl::CivilSecond date(2015, 2, 3, 4, 5, 6);
  absl::Time input_time = absl::FromCivil(date, time_zone);
  google::type::TimeOfDay* tod_message =
      Arena::Create<google::type::TimeOfDay>(&arena);

  tod_message->set_seconds(date.second());
  tod_message->set_minutes(date.minute());
  tod_message->set_hours(date.hour());

  PerformGetTimeOfDayUTCTest(&arena, input_time, &result);
  const google::type::TimeOfDay* time_of_day_tod =
      google::protobuf::DynamicCastMessage<const google::type::TimeOfDay>(
          result.MessageOrDie());

  ASSERT_EQ(time_of_day_tod->seconds(), tod_message->seconds());
  ASSERT_EQ(time_of_day_tod->minutes(), tod_message->minutes());
  ASSERT_EQ(time_of_day_tod->hours(), tod_message->hours());
}

TEST_F(ExtensionTest, TestBetweenToD) {
  Arena arena;
  CelValue result;
  google::type::TimeOfDay* time_of_day =
      Arena::Create<google::type::TimeOfDay>(&arena);
  google::type::TimeOfDay* start =
      Arena::Create<google::type::TimeOfDay>(&arena);
  google::type::TimeOfDay* stop =
      Arena::Create<google::type::TimeOfDay>(&arena);

  start->set_hours(20);
  start->set_minutes(0);
  start->set_seconds(0);
  stop->set_hours(21);
  stop->set_minutes(0);
  stop->set_seconds(0);
  time_of_day->set_hours(20);
  time_of_day->set_minutes(30);
  time_of_day->set_seconds(0);

  PerformBetweenToDTest(&arena, time_of_day, start, stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  time_of_day->set_minutes(0);
  PerformBetweenToDTest(&arena, time_of_day, start, stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  time_of_day->set_hours(19);
  PerformBetweenToDTest(&arena, time_of_day, start, stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);

  time_of_day->set_hours(21);
  PerformBetweenToDTest(&arena, time_of_day, start, stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);

  time_of_day->set_seconds(1);
  PerformBetweenToDTest(&arena, time_of_day, start, stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);
}

TEST_F(ExtensionTest, TestBetweenTodStr) {
  Arena arena;
  CelValue result;
  std::string start = "18:20:30";
  std::string stop = "19:20:30";
  google::type::TimeOfDay* time_of_day =
      Arena::Create<google::type::TimeOfDay>(&arena);

  time_of_day->set_hours(19);
  time_of_day->set_minutes(0);
  time_of_day->set_seconds(0);

  PerformBetweenToDStrTest(&arena, time_of_day, &start, &stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  time_of_day->set_hours(18);
  time_of_day->set_minutes(20);
  time_of_day->set_seconds(30);

  PerformBetweenToDStrTest(&arena, time_of_day, &start, &stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  time_of_day->set_seconds(29);

  PerformBetweenToDStrTest(&arena, time_of_day, &start, &stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);

  time_of_day->set_hours(19);
  time_of_day->set_minutes(20);
  time_of_day->set_seconds(30);

  PerformBetweenToDStrTest(&arena, time_of_day, &start, &stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);

  time_of_day->set_seconds(29);

  PerformBetweenToDStrTest(&arena, time_of_day, &start, &stop, &result);
  ASSERT_EQ(result.BoolOrDie(), true);

  time_of_day->set_seconds(31);

  PerformBetweenToDStrTest(&arena, time_of_day, &start, &stop, &result);
  ASSERT_EQ(result.BoolOrDie(), false);
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
