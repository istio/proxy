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

#include "common/constant.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/strings.h"

namespace cel {

const BytesConstant& BytesConstant::default_instance() {
  static const absl::NoDestructor<BytesConstant> instance;
  return *instance;
}

const StringConstant& StringConstant::default_instance() {
  static const absl::NoDestructor<StringConstant> instance;
  return *instance;
}

const Constant& Constant::default_instance() {
  static const absl::NoDestructor<Constant> instance;
  return *instance;
}

std::string FormatNullConstant() { return "null"; }

std::string FormatBoolConstant(bool value) {
  return value ? std::string("true") : std::string("false");
}

std::string FormatIntConstant(int64_t value) { return absl::StrCat(value); }

std::string FormatUintConstant(uint64_t value) {
  return absl::StrCat(value, "u");
}

std::string FormatDoubleConstant(double value) {
  if (std::isfinite(value)) {
    if (std::floor(value) != value) {
      // The double is not representable as a whole number, so use
      // absl::StrCat which will add decimal places.
      return absl::StrCat(value);
    }
    // absl::StrCat historically would represent 0.0 as 0, and we want the
    // decimal places so ZetaSQL correctly assumes the type as double
    // instead of int64.
    std::string stringified = absl::StrCat(value);
    if (!absl::StrContains(stringified, '.')) {
      absl::StrAppend(&stringified, ".0");
    }
    return stringified;
  }
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::signbit(value)) {
    return "-infinity";
  }
  return "+infinity";
}

std::string FormatBytesConstant(absl::string_view value) {
  return internal::FormatBytesLiteral(value);
}

std::string FormatStringConstant(absl::string_view value) {
  return internal::FormatStringLiteral(value);
}

std::string FormatDurationConstant(absl::Duration value) {
  return absl::StrCat("duration(\"", absl::FormatDuration(value), "\")");
}

std::string FormatTimestampConstant(absl::Time value) {
  return absl::StrCat(
      "timestamp(\"",
      absl::FormatTime("%Y-%m-%d%ET%H:%M:%E*SZ", value, absl::UTCTimeZone()),
      "\")");
}

}  // namespace cel
