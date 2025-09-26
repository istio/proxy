// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/input_model_helpers.h"

#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/input_model.h"

namespace ocpdiag::results {

std::string CommandLineStringFromMainArgs(int argc, const char* argv[]) {
  std::string command_line = argv[0];
  for (int i = 1; i < argc; i++) absl::StrAppend(&command_line, " ", argv[i]);
  return command_line;
}

std::string ParameterJsonFromMainArgs(int argc, const char* argv[]) {
  if (argc <= 1) return "{}";

  std::string parameter_json = "{";
  for (int i = 1; i < argc; i += 2) {
    while (argv[i][0] == '-') argv[i]++;
    absl::StrAppend(&parameter_json,
                    absl::StrFormat("\"%s\":\"%s\"", argv[i], argv[i + 1]));
    if (i + 2 < argc) absl::StrAppend(&parameter_json, ",");
  }
  absl::StrAppend(&parameter_json, "}");
  return parameter_json;
}

std::vector<Validator> ValidateWithinInclusiveLimits(double lower_limit,
                                                     double upper_limit,
                                                     absl::string_view name) {
  CHECK(lower_limit <= upper_limit) << "Tried to create a validator limit set "
                                       "with a lower limit that exceeds the "
                                       "upper limit";
  return {Validator{
              .type = ValidatorType::kGreaterThanOrEqual,
              .value = {lower_limit},
              .name = name.empty() ? "" : absl::StrCat(name, " Lower"),
          },
          Validator{
              .type = ValidatorType::kLessThanOrEqual,
              .value = {upper_limit},
              .name = name.empty() ? "" : absl::StrCat(name, " Upper"),
          }};
}

std::vector<Validator> ValidateWithinExclusiveLimits(double lower_limit,
                                                     double upper_limit,
                                                     absl::string_view name) {
  CHECK(lower_limit < upper_limit) << "Tried to create a validator limit set "
                                      "with a lower limit that exceeds the "
                                      "upper limit";
  return {Validator{
              .type = ValidatorType::kGreaterThanOrEqual,
              .value = {lower_limit},
              .name = name.empty() ? "" : absl::StrCat(name, " Lower"),
          },
          Validator{
              .type = ValidatorType::kLessThan,
              .value = {upper_limit},
              .name = name.empty() ? "" : absl::StrCat(name, " Upper"),
          }};
}

}  // namespace ocpdiag::results
