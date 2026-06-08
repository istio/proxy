// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/struct_validators.h"

#include <variant>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/variant.h"

namespace ocpdiag::results {

void ValidateStructOrDie(const Validator& validator) {
  absl::string_view identifier = validator.name;
  if (identifier.empty()) identifier = "Unnamed Validator";
  CHECK(!validator.value.empty())
      << "At least one value must be specified for validator: " << identifier;
  int type_index = validator.value[0].index();
  for (const Variant& variant : validator.value) {
    CHECK(variant.index() == type_index)
        << "All values must be of the same type for validator: " << identifier;
  }

  switch (validator.type) {
    case ValidatorType::kEqual:
    case ValidatorType::kNotEqual:
      CHECK(validator.value.size() == 1)
          << "Must specify exactly one value for EQUAL or NOT EQUAL validator: "
          << identifier;
      break;
    case ValidatorType::kLessThan:
    case ValidatorType::kLessThanOrEqual:
    case ValidatorType::kGreaterThan:
    case ValidatorType::kGreaterThanOrEqual:
      CHECK(validator.value.size() == 1)
          << "Must specify exactly one value for numerical comparison type "
             "validator: "
          << identifier;
      CHECK(std::holds_alternative<double>(validator.value[0]))
          << "Value must be numerical for numerical comparison validator: "
          << identifier;
      break;
    case ValidatorType::kRegexMatch:
    case ValidatorType::kRegexNoMatch:
      CHECK(std::holds_alternative<std::string>(validator.value[0]))
          << "Value must be a string or string collection for REGEX validator: "
          << identifier;
      break;
    case ValidatorType::kInSet:
    case ValidatorType::kNotInSet:
      CHECK(std::holds_alternative<std::string>(validator.value[0]) ||
            std::holds_alternative<double>(validator.value[0]))
          << "Value must be a string or numerical type for set validator: "
          << identifier;
      break;
    default:
      LOG(FATAL) << "Must specify type for validator: " << identifier;
      break;
  }
}

void ValidateStructOrDie(const HardwareInfo& hardware_info) {
  CHECK(!hardware_info.name.empty())
      << "Must specify the name field of the hardware info struct";
}

void ValidateStructOrDie(const SoftwareInfo& software_info) {
  CHECK(!software_info.name.empty())
      << "Must specify the name field of the software info struct";
}

void ValidateStructOrDie(const PlatformInfo& platform_info) {
  CHECK(!platform_info.info.empty())
      << "Must specify the info field of the platform info struct";
}

void ValidateStructOrDie(const Subcomponent& subcomponent) {
  CHECK(!subcomponent.name.empty())
      << "Must specify the name field of the subcomponent struct";
}

void ValidateStructOrDie(
    const MeasurementSeriesStart& measurement_series_start) {
  CHECK(!measurement_series_start.name.empty())
      << "Must specify the name field of the measurement series start struct";
  if (measurement_series_start.subcomponent.has_value())
    ValidateStructOrDie(*measurement_series_start.subcomponent);

  if (measurement_series_start.validators.empty()) return;
  int type_index = measurement_series_start.validators[0].value[0].index();
  for (const Validator& validator : measurement_series_start.validators) {
    ValidateStructOrDie(validator);
    CHECK(type_index == -1 || type_index == validator.value[0].index())
        << "All validators must be the same type for measurement series start: "
        << measurement_series_start.name;
  }
}

void ValidateStructOrDie(const Measurement& measurement) {
  CHECK(!measurement.name.empty())
      << "Must specify the name field of the measurement struct";
  if (measurement.subcomponent.has_value())
    ValidateStructOrDie(*measurement.subcomponent);

  int type_index = measurement.value.index();
  for (const Validator& validator : measurement.validators) {
    ValidateStructOrDie(validator);
    CHECK(type_index == validator.value[0].index())
        << "All validators and the value must be the same type for "
           "measurement: "
        << measurement.name;
  }
}

void ValidateStructOrDie(const Diagnosis& diagnosis) {
  CHECK(!diagnosis.verdict.empty())
      << "Must specify the verdict field of the diagnosis struct";
  CHECK(diagnosis.type != DiagnosisType::kUnknown)
      << "Must specify a type for all diagnoses";
  if (diagnosis.subcomponent.has_value())
    ValidateStructOrDie(*diagnosis.subcomponent);
}

void ValidateStructOrDie(const Error& error) {
  CHECK(!error.symptom.empty())
      << "Must specify the symptom field of the error struct";
}

void ValidateStructOrDie(const Log& log) {
  CHECK(!log.message.empty()) << "Must specify the message field of the log";
}

void ValidateStructOrDie(const File& file) {
  CHECK(!file.display_name.empty())
      << "Must specify the display name of the file struct";
  CHECK(!file.uri.empty()) << "Must specify the URI of the file struct: "
                           << file.display_name;
  //
  // implemented
}

void ValidateStructOrDie(const TestRunStart& test_run_info) {
  CHECK(!test_run_info.name.empty())
      << "Must specify the name of the test run info";
  CHECK(!test_run_info.version.empty())
      << "Must specify the version in the test run info: "
      << test_run_info.name;
  CHECK(!test_run_info.command_line.empty())
      << "Must specify the command line invocation in the test run info: "
      << test_run_info.name;
}

void ValidateStructOrDie(const Extension& extension) {
  CHECK(!extension.name.empty()) << "Must specify the name of the extension";
  CHECK(!extension.content_json.empty())
      << "Must specify the content of the extension";
}

}  // namespace ocpdiag::results
